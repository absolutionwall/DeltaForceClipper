@echo off
setlocal
set "SCRIPT_DIR=%~dp0"

echo ===================================================
echo  Delta Force Clipper - Release y Push a GitHub
echo ===================================================
echo.
echo Selecciona el tipo de incremento de version:
echo   [1] PATCH (0.1.0 -^> 0.1.1) - Arreglos y mejoras menores
echo   [2] MINOR (0.1.0 -^> 0.2.0) - Nuevas funciones
echo   [3] MAJOR (0.1.0 -^> 1.0.0) - Cambios mayores
echo.
set /p "OPCION=Elige una opcion [1-3] (Enter = 1): "

set "BUMP=patch"
if "%OPCION%"=="2" set "BUMP=minor"
if "%OPCION%"=="3" set "BUMP=major"

echo.
echo Ejecutando release (%BUMP%) y subiendo a GitHub...
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\release.ps1" -Bump %BUMP% -PushGitHub

set "RESULT=%ERRORLEVEL%"
echo.
if not "%RESULT%"=="0" (
    echo [ERROR] El proceso de release fallo. Revisa el mensaje de arriba.
) else (
    echo [OK] Release compilada, etiquetada y subida a GitHub correctamente.
)

pause
