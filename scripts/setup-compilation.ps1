param(
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot "build_x64"

function Invoke-CheckedCommand {
    param(
        [string]$Name,
        [string[]]$Arguments
    )

    & $Name @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

try {
    Push-Location $ProjectRoot
    Write-Host "Delta Force Clipper - configure, build, and package" -ForegroundColor Cyan

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw "CMake 3.28+ was not found. Install CMake and ensure it is on PATH."
    }
    if (-not (Get-Command cpack -ErrorAction SilentlyContinue)) {
        throw "CPack was not found. Install CMake with CPack and ensure it is on PATH."
    }

    $bootstrap = Join-Path $ProjectRoot "cmake\common\bootstrap.cmake"
    if (-not (Test-Path $bootstrap)) {
        Write-Host "OBS CMake infrastructure is missing; bootstrapping from the official template..." -ForegroundColor Yellow
        $setupScript = Join-Path $PSScriptRoot "setup-dev.ps1"
        & $setupScript -TemplateDir (Join-Path $ProjectRoot "obs-plugintemplate")
        if (-not (Test-Path $bootstrap)) {
            throw "Could not prepare OBS CMake infrastructure."
        }
    }

    Write-Host "`n[1/3] Configuring..." -ForegroundColor Cyan
    Invoke-CheckedCommand "cmake" @("--preset", "windows-x64")

    Write-Host "`n[2/3] Building RelWithDebInfo..." -ForegroundColor Cyan
    Invoke-CheckedCommand "cmake" @("--build", "--preset", "windows-x64", "--parallel")

    Write-Host "`n[3/3] Creating ZIP package..." -ForegroundColor Cyan
    Invoke-CheckedCommand "cpack" @(
        "--config", (Join-Path $BuildDirectory "CPackConfig.cmake"),
        "-C", "RelWithDebInfo",
        "-B", $BuildDirectory
    )

    $packages = Get-ChildItem -Path $BuildDirectory -Filter "deltaforce-clipper-*.zip" -File |
        Sort-Object LastWriteTime -Descending
    if (-not $packages) {
        throw "CPack completed but no deltaforce-clipper ZIP was found in $BuildDirectory."
    }

    Write-Host "`nBuild and packaging succeeded." -ForegroundColor Green
    Write-Host "Package: $($packages[0].FullName)" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "`nBuild failed: $($_.Exception.Message)" -ForegroundColor Red
    if (-not $NoPause) {
        Read-Host "Press Enter to close"
    }
    exit 1
}
finally {
    Pop-Location -ErrorAction SilentlyContinue
}
