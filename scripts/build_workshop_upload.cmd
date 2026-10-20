@echo off
rem Builds dist\workshop_upload.exe (tools\WorkshopUpload\workshop_upload.cpp) with
rem the VS2022 Build Tools x64 compiler. A standalone tool: it does not need the
rem v100 toolchain or KenshiLib. Used by scripts\publish_workshop.ps1.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
    echo No se encontro VS2022 Build Tools ^(vcvars64.bat^).
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "ROOT=%~dp0.."
if not exist "%ROOT%\dist" mkdir "%ROOT%\dist"
cl /nologo /EHsc /O2 /W3 /D_CRT_SECURE_NO_WARNINGS "%ROOT%\tools\WorkshopUpload\workshop_upload.cpp" ^
   /Fo"%TEMP%\workshop_upload.obj" /Fe"%ROOT%\dist\workshop_upload.exe" || exit /b 1
echo workshop_upload built: %ROOT%\dist\workshop_upload.exe
