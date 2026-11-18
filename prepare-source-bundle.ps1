$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$destination = Join-Path $PSScriptRoot 'dist\PrismSources'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$qtVersion = '6.12.0'
$manifest = @()
foreach ($module in 'qtbase','qtdeclarative','qtwebchannel','qtpositioning','qtsvg','qtimageformats','qtquick3d','qtwebengine') {
    if ((Get-PSDrive C).Free -lt 20GB) { throw 'Source download stopped to preserve free disk space.' }
    $version = if ($module -eq 'qtwebengine') { '6.140.0' } else { $qtVersion }
    $name = "$module-everywhere-src-$version.tar.xz"
    $url = if ($module -eq 'qtwebengine') {
        "https://download.qt.io/archive/qtwebengine/6.140/$version/src/$name"
    } else { "https://download.qt.io/archive/qt/6.12/$version/submodules/$name" }
    $checksum = (Invoke-WebRequest -Uri ($url + '.sha256') -TimeoutSec 60).Content
    if ($checksum -is [byte[]]) { $checksum = [Text.Encoding]::UTF8.GetString($checksum) }
    $match = [regex]::Match([string]$checksum, '(?im)^([a-f0-9]{64})(?:\s|$)')
    if (!$match.Success) { throw "No valid upstream SHA-256 for $module." }
    $expected = $match.Groups[1].Value.ToUpperInvariant()
    $archive = Join-Path $destination $name
    if (Test-Path -LiteralPath $archive) {
        if ((Get-FileHash -LiteralPath $archive).Hash -ne $expected) { throw "Existing source archive does not match upstream: $name" }
    } else {
        $temporary = $archive + '.download'
        Invoke-WebRequest -Uri $url -OutFile $temporary -TimeoutSec 600
        if ((Get-FileHash -LiteralPath $temporary).Hash -ne $expected) { throw "Source checksum mismatch: $name" }
        Move-Item -LiteralPath $temporary -Destination $archive
    }
    $manifest += [ordered]@{ module=$module; version=$version; url=$url; file=$name; sha256=$expected }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $destination 'qt-source-manifest.json') -Encoding utf8
    Write-Output "Verified source: $module $version"
}
Write-Output 'Qt module sources verified. Custom WebEngine patch/provenance and public codec-rights acceptance remain separate.'
