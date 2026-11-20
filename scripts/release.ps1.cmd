param(
    [ValidateSet("patch", "minor", "major")]
    [string]$Bump = "patch",
    [switch]$PushGitHub
)

$ErrorActionPreference = "Stop"
$rootDir   = Resolve-Path "$PSScriptRoot\.."
$cmakeFile = Join-Path $rootDir "CMakeLists.txt"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

# 1. Leer la versión actual desde CMakeLists.txt
$cmakeContent = [System.IO.File]::ReadAllText($cmakeFile)
if ($cmakeContent -match '(?m)(VERSION\s+|set\(_version\s+")(\d+)\.(\d+)\.(\d+)') {
    $prefix = $Matches
    $major  = [int]$Matches
    $minor  = [int]$Matches
    $patch  = [int]$Matches
    $oldVer = "$major.$minor.$patch"
} else {
    throw "No se encontro el formato de version X.Y.Z en CMakeLists.txt"
}

# 2. Calcular nueva versión (+1 según el tipo elegido)
switch ($Bump) {
    "major" { $major++; $minor = 0; $patch = 0 }
    "minor" { $minor++; $patch = 0 }
    "patch" { $patch++ }
}
$newVer = "$major.$minor.$patch"
Write-Host "=== SUBIENDO VERSION: $oldVer -> $newVer ===" -ForegroundColor Cyan

# 3. Actualizar CMakeLists.txt (sin BOM)
$updatedCmake = $cmakeContent -replace "(?m)($([regex]::Escape($prefix)))$([regex]::Escape($oldVer))", "`${1}$newVer"
[System.IO.File]::WriteAllText($cmakeFile, $updatedCmake, $utf8NoBom)

# 4. Compilar y generar paquete ZIP
Set-Location $rootDir
& "$PSScriptRoot\setup-compilation.ps1" -NoPause

# 5. Guardar copia del ZIP en la carpeta local 'releases/'
$releasesDir = Join-Path $rootDir "releases"
if (-not (Test-Path $releasesDir)) { New-Item -ItemType Directory -Path $releasesDir | Out-Null }

$zipFile = Get-ChildItem "$rootDir\deltaforce-clipper-$newVer-*.zip" | Select-Object -First 1
if (-not $zipFile) {
    $zipFile = Get-ChildItem "$rootDir\build_x64\deltaforce-clipper-$newVer-*.zip" | Select-Object -First 1
}
if ($zipFile) {
    Copy-Item $zipFile.FullName $releasesDir -Force
    Write-Host "ZIP guardado en: $releasesDir\$($zipFile.Name)" -ForegroundColor Green
}

# 6. Registrar commit y Tag en Git
git add CMakeLists.txt
git commit -m "chore(release): v$newVer"
git tag -a "v$newVer" -m "Release v$newVer"
Write-Host "Tag de Git creado: v$newVer" -ForegroundColor Green

# 7. Subir a GitHub y publicar Release (si se pasó -PushGitHub y tienes 'gh' instalado)
if ($PushGitHub) {
    git push origin main --tags
    if ((Get-Command gh -ErrorAction SilentlyContinue) -and $zipFile) {
        gh release create "v$newVer" "$releasesDir\$($zipFile.Name)" --title "v$newVer" --notes "Release automatica v$newVer"
        Write-Host "Publicado en GitHub Releases!" -ForegroundColor Cyan
    }
}
