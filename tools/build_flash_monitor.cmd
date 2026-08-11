@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_flash_monitor.ps1" %*
exit /b %ERRORLEVEL%
