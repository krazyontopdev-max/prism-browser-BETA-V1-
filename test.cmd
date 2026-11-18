@echo off
setlocal
cd /d "%~dp0"
set "PATH=C:\Qt\6.12.0\msvc2022_64\bin;%PATH%"
if /i "%~1"=="network" set "PRISM_NETWORK_TESTS=1"
"C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build --output-on-failure
exit /b %errorlevel%
