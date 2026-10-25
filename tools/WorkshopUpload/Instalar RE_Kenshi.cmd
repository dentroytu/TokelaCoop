@echo off
rem TokelaCoop from the Steam Workshop: installs RE_Kenshi (the mod loader TokelaCoop
rem needs; the Workshop cannot) and enables TokelaCoop. Double-click this file.
rem Runs installer\Install-TokelaCoop.ps1 -Workshop (plain text - open it to see what it does).
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\Install-TokelaCoop.ps1" -Workshop %*
exit /b %errorlevel%
