param([switch]$Installer)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$package = Join-Path $PSScriptRoot $(if ($Installer) { 'dist\PrismInstallerStage' } else { 'dist\PrismPreview' })
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath 'build\Prism.exe' -Destination $package -Force
Copy-Item -LiteralPath 'resources\qt.conf' -Destination $package -Force
& 'C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe' --release --no-translations --no-compiler-runtime (Join-Path $package 'Prism.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
New-Item -ItemType Directory -Path (Join-Path $package 'imageformats') -Force | Out-Null
Copy-Item -LiteralPath 'third_party\qt-webp\plugins\imageformats\qwebp.dll' -Destination (Join-Path $package 'imageformats') -Force
New-Item -ItemType Directory -Path (Join-Path $package 'licenses') -Force | Out-Null
$webpNotices = Join-Path $package 'licenses\qt-webp'
New-Item -ItemType Directory -Path $webpNotices -Force | Out-Null
foreach ($notice in 'COPYING','GPL-3.0-only.txt','LGPL-3.0-only.txt','README.md') { Copy-Item -LiteralPath (Join-Path 'third_party\qt-webp' $notice) -Destination $webpNotices -Force }
$qtNotices = Join-Path $package 'licenses\qt'
New-Item -ItemType Directory -Path $qtNotices -Force | Out-Null
Copy-Item -Path 'third_party\qt-licenses\*' -Destination $qtNotices -Force
Copy-Item -LiteralPath 'wallpapers' -Destination $package -Recurse -Force
Copy-Item -LiteralPath 'third_party\filters' -Destination $package -Recurse -Force
$vs = & 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -property installationPath
$crt = Join-Path $vs 'VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT'
if (!(Test-Path -LiteralPath $crt)) { throw 'Matching installed MSVC runtime redistributables were not found.' }
foreach ($dll in Get-ChildItem -LiteralPath $crt -Filter '*.dll') {
    if ((Get-AuthenticodeSignature -LiteralPath $dll.FullName).Status -ne 'Valid') { throw "Invalid runtime signature: $($dll.Name)" }
    Copy-Item -LiteralPath $dll.FullName -Destination $package -Force
}
foreach ($notice in 'LICENSE','THIRD-PARTY-NOTICES.md','PRIVACY.md','CONTRIBUTING.md','README.md') { Copy-Item -LiteralPath $notice -Destination $package -Force }
Copy-Item -LiteralPath 'docs\v1-acceptance.md' -Destination (Join-Path $package 'ACCEPTANCE.md') -Force
Copy-Item -LiteralPath 'docs\v08-release-work.md' -Destination (Join-Path $package 'RELEASE-WORK.md') -Force
Compress-Archive -LiteralPath $package -DestinationPath (Join-Path $PSScriptRoot 'dist\Prism-Browser-1.0.0-Windows-x64.zip') -CompressionLevel Fastest -Force
Write-Output 'Prism Browser packaged. Public release acceptance remains gated.'
