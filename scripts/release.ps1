param(
    [ValidateSet("patch", "minor", "major")]
    [string]$Bump = "patch",
    [switch]$PushGitHub
)

$ErrorActionPreference = "Stop"
$rootDir   = Resolve-Path "$PSScriptRoot\.."
$cmakeFile = Join-Path $rootDir "CMakeLists.txt"
$specFile  = Join-Path $rootDir "buildspec.json"
$suppFile  = Join-Path $rootDir "src\plugin-support.h"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

# 1. Leer la versión actual desde CMakeLists.txt
$cmakeContent = [System.IO.File]::ReadAllText($cmakeFile)
$m = [regex]::Match($cmakeContent, 'project\(DeltaForceClipper\s+VERSION\s+(\d+\.\d+\.\d+)\)')
if ($m.Success) {
    $oldVer = ($m.Value -replace '[^\d\.]', '')
    $nums   = $oldVer.Split('.')
    $major  = [int]$nums.Item(0)
    $minor  = [int]$nums.Item(1)
    $patch  = [int]$nums.Item(2)
} else {
    throw "No se encontro 'project(DeltaForceClipper VERSION X.Y.Z)' en CMakeLists.txt"
}

# 2. Calcular nueva versión (+1 según el tipo elegido)
switch ($Bump) {
    "major" { $major++; $minor = 0; $patch = 0 }
    "minor" { $minor++; $patch = 0 }
    "patch" { $patch++ }
}
$newVer = "$major.$minor.$patch"
Write-Host "=== SUBIENDO VERSION: $oldVer -> $newVer ===" -ForegroundColor Cyan

# 3. Pedir descripción breve de los cambios (opcional)
$notas = Read-Host "Descripcion de esta version (Enter para mensaje por defecto)"
if ([string]::IsNullOrWhiteSpace($notas)) {
    $notas = "Release v$newVer"
}

# 4. Actualizar los 3 archivos del proyecto (sin BOM)
$updatedCmake = $cmakeContent -replace "(project\(DeltaForceClipper\s+VERSION\s+)$([regex]::Escape($oldVer))", "`${1}$newVer"
[System.IO.File]::WriteAllText($cmakeFile, $updatedCmake, $utf8NoBom)

if (Test-Path $specFile) {
    $specContent = [System.IO.File]::ReadAllText($specFile)
    $updatedSpec = $specContent -replace '("version"\s*:\s*")' + [regex]::Escape($oldVer) + '"', "`${1}$newVer`""
    [System.IO.File]::WriteAllText($specFile, $updatedSpec, $utf8NoBom)
}

if (Test-Path $suppFile) {
    $suppContent = [System.IO.File]::ReadAllText($suppFile)
    $updatedSupp = $suppContent -replace '(#define\s+PLUGIN_VERSION\s+")' + [regex]::Escape($oldVer) + '"', "`${1}$newVer`""
    [System.IO.File]::WriteAllText($suppFile, $updatedSupp, $utf8NoBom)
}

Write-Host "Archivos actualizados: CMakeLists.txt, buildspec.json y plugin-support.h" -ForegroundColor Green

# 5. Compilar y generar paquete ZIP
Set-Location $rootDir
& "$PSScriptRoot\setup-compilation.ps1" -NoPause

# 6. Guardar copia del ZIP en la carpeta local 'releases/'
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

# 7. Preparar cambios del proyecto (respetando .gitignore) y crear Commit + Tag
git add .
git commit -m "release(v$newVer): $notas"
git tag -a "v$newVer" -m "$notas"
Write-Host "Commit y Tag creados: v$newVer" -ForegroundColor Green

# 8. Subir código, tags y publicar Release con el ZIP en GitHub
if ($PushGitHub) {
    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    git push origin $branch --tags
    if ((Get-Command gh -ErrorAction SilentlyContinue) -and $zipFile) {
        gh release create "v$newVer" "$releasesDir\$($zipFile.Name)" --title "v$newVer - $notas" --notes "$notas"
        Write-Host "Publicado en GitHub Releases con su ZIP adjunto!" -ForegroundColor Cyan
    }
}
