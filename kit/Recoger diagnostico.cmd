@echo off
rem TokelaCoop: collects logs, mod list and versions into a zip on the Desktop.
rem Runs installer\Collect-Diagnostics.ps1 (plain text - open it to see what it does).
setlocal
cd /d "%~dp0"
if not exist "%~dp0installer\Collect-Diagnostics.ps1" (echo Descomprime el zip del kit entero y vuelve a probar. / Extract the whole kit zip and try again. & pause & exit /b 1)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\Collect-Diagnostics.ps1" %*
exit /b %errorlevel%
