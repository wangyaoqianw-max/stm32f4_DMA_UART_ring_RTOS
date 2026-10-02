@echo off
setlocal EnableExtensions

set "TOOLKIT_PS1=%~dp0toolkit.ps1"
if not exist "%TOOLKIT_PS1%" (
    echo [TOOLKIT][ERROR] Router not found: %TOOLKIT_PS1%
    exit /b 10
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TOOLKIT_PS1%" %*
exit /b %ERRORLEVEL%
