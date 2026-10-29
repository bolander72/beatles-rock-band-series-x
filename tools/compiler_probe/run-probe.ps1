$ErrorActionPreference='Stop'
$probeRoot=$PSScriptRoot
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual C++ tools not found'}
$probeBatch=@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
python "$probeRoot\run_probe.py" --root "$probeRoot"
exit /b %errorlevel%
"@
Set-Content "$probeRoot\compile-probe.cmd" $probeBatch
cmd /d /c "$probeRoot\compile-probe.cmd"
if($LASTEXITCODE -ne 0){throw 'Compiler probe harness failed'}
