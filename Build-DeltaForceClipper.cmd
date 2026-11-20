@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\setup-compilation.ps1" -NoPause
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
  echo.
  echo Build failed. See the error above.
) else (
  echo.
  echo Build and ZIP package completed successfully.
)
pause
exit /b %RESULT%
