@echo off
setlocal
cd /d "%~dp0"
if not exist "build\Prism.exe" (
    echo Run build.cmd first.
    exit /b 1
)
if not exist "dist\Prism" mkdir "dist\Prism"
copy /y "build\Prism.exe" "dist\Prism\Prism.exe" >nul
if errorlevel 1 exit /b 1
"C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime "dist\Prism\Prism.exe"
if errorlevel 1 exit /b 1
if not exist "dist\Prism\imageformats" mkdir "dist\Prism\imageformats"
copy /y "third_party\qt-webp\plugins\imageformats\qwebp.dll" "dist\Prism\imageformats\qwebp.dll" >nul
if errorlevel 1 exit /b 1
if not exist "dist\Prism\licenses\qt-webp" mkdir "dist\Prism\licenses\qt-webp"
copy /y "third_party\qt-webp\*.txt" "dist\Prism\licenses\qt-webp\" >nul
copy /y "third_party\qt-webp\COPYING" "dist\Prism\licenses\qt-webp\" >nul
copy /y "third_party\qt-webp\README.md" "dist\Prism\licenses\qt-webp\" >nul
if errorlevel 1 exit /b 1
xcopy "wallpapers\images\*.webp" "dist\Prism\wallpapers\images\" /I /Y /Q >nul
if errorlevel 1 exit /b 1
xcopy "wallpapers\licenses\*.html" "dist\Prism\wallpapers\licenses\" /I /Y /Q >nul
if errorlevel 1 exit /b 1
copy /y "wallpapers\catalog.json" "dist\Prism\wallpapers\catalog.json" >nul
if errorlevel 1 exit /b 1
copy /y "wallpapers\CREDITS.html" "dist\Prism\wallpapers\CREDITS.html" >nul
if errorlevel 1 exit /b 1
copy /y "wallpapers\sources.json" "dist\Prism\wallpapers\sources.json" >nul
if errorlevel 1 exit /b 1
xcopy "third_party\filters\*" "dist\Prism\filters\" /I /Y /Q >nul
exit /b %errorlevel%
