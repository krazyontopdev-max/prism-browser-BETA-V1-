@echo off
setlocal
cd /d "%~dp0"
if exist "dist\PrismPreview\Prism.exe" (
    start "Prism Browser" "dist\PrismPreview\Prism.exe" %*
    exit /b 0
)
if exist "dist\Prism\Prism.exe" (
    start "Prism" "dist\Prism\Prism.exe" %*
    exit /b 0
)
if not exist "build\Prism.exe" (
    echo Prism has not been built. Run build.cmd first.
    pause
    exit /b 1
)
set "PATH=C:\Qt\6.12.0\msvc2022_64\bin;%PATH%"
start "Prism" "build\Prism.exe" %*
