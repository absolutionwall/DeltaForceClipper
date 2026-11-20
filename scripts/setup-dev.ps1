param(
    [string]$TemplateDir = "obs-plugintemplate"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

if (-not [System.IO.Path]::IsPathRooted($TemplateDir)) {
    $TemplateDir = Join-Path $ProjectRoot $TemplateDir
}

Write-Host "Delta Force Clipper - development bootstrap" -ForegroundColor Cyan

if (-not (Test-Path $TemplateDir)) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "Git is required to download the official OBS plugin template."
    }
    Write-Host "Cloning official OBS plugin template..." -ForegroundColor Yellow
    git clone --depth 1 https://github.com/obsproject/obs-plugintemplate.git $TemplateDir
    if ($LASTEXITCODE -ne 0) {
        throw "Could not clone the official OBS plugin template."
    }
}

if (-not (Test-Path (Join-Path $TemplateDir "cmake\common\bootstrap.cmake"))) {
    throw "The OBS template is missing cmake\common\bootstrap.cmake."
}

Write-Host "Copying OBS CMake infrastructure to project root..."
$destination = Join-Path $ProjectRoot "cmake"
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -Path (Join-Path $TemplateDir "cmake\*") -Destination $destination -Recurse -Force

if (-not (Test-Path (Join-Path $destination "common\bootstrap.cmake"))) {
    throw "OBS CMake infrastructure was not copied correctly."
}
Write-Host "Infrastructure prepared." -ForegroundColor Green
