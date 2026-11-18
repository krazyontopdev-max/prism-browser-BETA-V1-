$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$scratch = Join-Path $project ('build\launcher-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products '*' -property installationPath
$source = Join-Path $scratch 'probe.cpp'
@'
#include <cstdlib>
#include <fstream>
int main(int argc, char **argv)
{
    const char *path = std::getenv("PRISM_LAUNCH_PROBE");
    if (!path) return 2;
    std::ofstream output(path);
    for (int i = 1; i < argc; ++i) output << argv[i] << '\n';
    return output ? 0 : 3;
}
'@ | Set-Content -LiteralPath $source -Encoding ascii
$probe = Join-Path $scratch 'probe.exe'
$compile = Join-Path $scratch 'compile.cmd'
@('@echo off',
    ('call "' + $vs + '\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44'),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /std:c++17 /EHsc /MT /D_CRT_SECURE_NO_WARNINGS /Fo:"' + $scratch + '\probe.obj" /Fe:"' + $probe + '" "' + $source + '" /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup'),
    'exit /b %errorlevel%') | Set-Content -LiteralPath $compile -Encoding ascii
& $compile *> (Join-Path $scratch 'compile.log')
if ($LASTEXITCODE -ne 0) { throw "Launcher fixture compilation failed; inspect $scratch\compile.log" }
$expected = @('--private','--profile','launcher-test','--probe-label','two words')
$results = @()
$previous = $env:PRISM_LAUNCH_PROBE
try {
    foreach ($branch in 'preview','portable','build') {
        $fixture = Join-Path $scratch $branch
        $relative = switch ($branch) { 'preview' { 'dist\PrismPreview' } 'portable' { 'dist\Prism' } 'build' { 'build' } }
        $folder = New-Item -ItemType Directory -Path (Join-Path $fixture $relative) -Force
        Copy-Item -LiteralPath $probe -Destination (Join-Path $folder.FullName 'Prism.exe')
        $launch = Join-Path $fixture 'launch.cmd'
        Copy-Item -LiteralPath (Join-Path $project 'launch.cmd') -Destination $launch
        $env:PRISM_LAUNCH_PROBE = Join-Path $fixture 'arguments.txt'
        $command = '/d /s /c ""' + $launch + '" --private --profile launcher-test --probe-label "two words""'
        $worker = Start-Process -FilePath $env:ComSpec -ArgumentList $command -WindowStyle Hidden -PassThru -Wait
        if ($worker.ExitCode -ne 0) { throw "Launcher failed for $branch." }
        for ($attempt = 0; $attempt -lt 50 -and !(Test-Path -LiteralPath $env:PRISM_LAUNCH_PROBE); ++$attempt) { Start-Sleep -Milliseconds 200 }
        $actual = if (Test-Path -LiteralPath $env:PRISM_LAUNCH_PROBE) { @(Get-Content -LiteralPath $env:PRISM_LAUNCH_PROBE) } else { @() }
        $passed = ($actual.Count -eq $expected.Count) -and (($actual -join "`n") -ceq ($expected -join "`n"))
        $results += [ordered]@{ branch=$branch; passed=$passed; expected=$expected; actual=@($actual) }
        Write-Output ("{0}: {1}" -f $branch, $(if ($passed) { 'PASS' } else { 'FAIL' }))
    }
} finally { $env:PRISM_LAUNCH_PROBE = $previous }
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $project 'build\launcher-tests-results.json') -Encoding utf8
if (@($results | Where-Object { !$_.passed }).Count) { throw 'Launcher did not preserve the supplied arguments.' }
