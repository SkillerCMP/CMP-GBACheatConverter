@echo off
setlocal EnableExtensions
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows-x64.ps1" %*
exit /b %ERRORLEVEL%
