param([int]$Port = 8765)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dashboard = Join-Path $root "dashboard"

python -m http.server $Port --directory $dashboard
