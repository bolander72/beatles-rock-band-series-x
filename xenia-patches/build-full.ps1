$ErrorActionPreference='Stop'
$root=(Get-Location).Path
$upstream='2e03062001c27c1f20010bb397783c8d66c58041'
git clone --depth 1 --branch 0.6.0.2 https://github.com/danprice142/xenia-canary-uwp.git xenia-source
if($LASTEXITCODE -ne 0){throw 'Upstream clone failed'}
if((git -C xenia-source rev-parse HEAD) -ne $upstream){throw 'Pinned upstream changed'}
git -C xenia-source submodule update --init --recursive --jobs 8
if($LASTEXITCODE -ne 0){throw 'Submodule checkout failed'}
New-Item build,xenia-artifacts -ItemType Directory -Force | Out-Null
Invoke-WebRequest 'https://raw.githubusercontent.com/microsoftconnect/GameInput/dce6dabd75b53fcb29db86afe4de5c917921af18/include/v0/GameInput.h' -OutFile build/GameInput.h
if((Get-FileHash build/GameInput.h -Algorithm SHA256).Hash.ToLower() -ne '4bda852f78a43f715837c1d97af7cf78fa14aa35dba4e755d305985364bf1196'){throw 'GameInput header checksum mismatch'}
Copy-Item xenia-source/src/xenia/apu/audio_system.cc build/audio_system.original.cc
python xenia-patches/prepare_full.py xenia-source
if($LASTEXITCODE -ne 0){throw 'Source preparation failed'}
python xenia-patches/prepare_launcher.py xenia-source
if($LASTEXITCODE -ne 0){throw 'Launcher preparation failed'}
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$compile=@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
python xenia-patches/test_audio_lifetime.py --original build/audio_system.original.cc --patched xenia-source/src/xenia/apu/audio_system.cc --compiler cl --work build/lifetime-tests
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX xenia-patches\full_mapping_test.cc /Fe:build\mapping-test.exe
if errorlevel 1 exit /b 1
build\mapping-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX xenia-patches\party_launch_test.cc /Fe:build\party-launch-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX xenia-patches\console_profiles_test.cc /Fe:build\console-profiles-test.exe
if errorlevel 1 exit /b 1
build\console-profiles-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX xenia-patches\console_profile_migration_test.cc /Fe:build\console-profile-migration-test.exe
if errorlevel 1 exit /b 1
build\console-profile-migration-test.exe
if errorlevel 1 exit /b 1
build\party-launch-test.exe
if errorlevel 1 exit /b 1
if not exist build\party-ui mkdir build\party-ui
cl /nologo /std:c++17 /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS /Ixenia-source\third_party\imgui /Ixenia-patches xenia-patches\party_ui_test.cc xenia-source\third_party\imgui\imgui.cpp xenia-source\third_party\imgui\imgui_draw.cpp xenia-source\third_party\imgui\imgui_widgets.cpp xenia-source\third_party\imgui\imgui_tables.cpp /Fo:build\party-ui\ /Fe:build\party-ui-test.exe /link user32.lib imm32.lib
if errorlevel 1 exit /b 1
build\party-ui-test.exe
if errorlevel 1 exit /b 1
python xenia-patches/test_guitar_ui_release.py --source xenia-source --compiler cl --work build/guitar-release-tests
exit /b %errorlevel%
"@
Set-Content build/tests.cmd $compile
cmd /d /c "$root\build\tests.cmd"
if($LASTEXITCODE -ne 0){throw 'Full mapping checks failed'}
Set-Location xenia-source
cmake --preset uwp
if($LASTEXITCODE -ne 0){throw 'UWP configure failed'}
# Compile the changed integration units with their real generated project
# settings before the much longer emulator build. SelectedFiles is the MSBuild
# single-file compile target; no project references or linking are needed here.
$msbuild="$vs\MSBuild\Current\Bin\MSBuild.exe"
$projects=@(Get-ChildItem build/uwp -Recurse -Filter *.vcxproj | Where-Object {$_.BaseName -in @('xenia-uwp','xenia-kernel')})
if($projects.Count -ne 2){throw 'Native account preflight projects not found'}
$preflightCount=0
$preflightFailures=@()
foreach($project in $projects){
  [xml]$description=Get-Content $project.FullName
  foreach($item in $description.SelectNodes("//*[local-name()='ClCompile' and @Include]")){
    if($item.Include -notmatch '[\\/](console_accounts\.cc|App\.cpp|emulator_window\.cc|xam_ui\.cc)$'){continue}
    & $msbuild $project.FullName /t:ClCompile /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SelectedFiles=$($item.Include)" /verbosity:minimal
    if($LASTEXITCODE -ne 0){$preflightFailures += $item.Include}
    ++$preflightCount
  }
}
if($preflightCount -ne 4){throw 'Expected all four native account integration units to compile'}
if($preflightFailures.Count){throw "Native account preflight failed: $($preflightFailures -join ', ')"}
Set-Location $root
python xenia-patches/archive_source.py xenia-source xenia-artifacts/corresponding-source.tar.gz
if($LASTEXITCODE -ne 0){throw 'Corresponding source archive failed'}
Set-Location xenia-source
cmake --build build/uwp --config Release --target riffinput -- /m:2 /verbosity:minimal
if($LASTEXITCODE -ne 0){throw 'Full input adapter compile failed'}
cmake --build --preset uwp-release -- /m:3 /p:CL_MPCount=2 /verbosity:minimal
if($LASTEXITCODE -ne 0){throw 'UWP build failed'}
Set-Location $root
# Repack to include the explicitly loaded input DLL even if the VS packager
# omits an import library whose exports are all resolved dynamically.
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory("$root\xenia-source\build\uwp\release\Xenia-Netplay-0.6.0.11.appx","$root\full-layout")
$adapter=Get-ChildItem xenia-source/build/uwp -Recurse -Filter riffinput.dll | Where-Object {$_.FullName -match 'Release'} | Select-Object -First 1
if(!$adapter){throw 'Built input adapter missing'}
Copy-Item $adapter.FullName full-layout/riffinput.dll -Force
New-Item full-layout/Licenses/Launcher -ItemType Directory -Force | Out-Null
Copy-Item LICENSE full-layout/Licenses/Launcher/LICENSE.txt
Copy-Item THIRD_PARTY_NOTICES.md full-layout/Licenses/Launcher/THIRD_PARTY_NOTICES.md
Remove-Item -LiteralPath full-layout/AppxBlockMap.xml,full-layout/AppxSignature.p7x,'full-layout/[Content_Types].xml',full-layout/AppxMetadata/CodeIntegrity.cat -Force -ErrorAction SilentlyContinue
$sdkRoot="${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk=Get-ChildItem "$sdkRoot\bin" -Directory | Where-Object {Test-Path "$($_.FullName)\x64\signtool.exe"} | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1
$package="$root\xenia-artifacts\Beatles-Rock-Band-Launcher-0.6.0.11-x64.appx"
& "$($sdk.FullName)\x64\makeappx.exe" pack /d full-layout /p $package /o /h SHA256
if($LASTEXITCODE -ne 0){throw 'Package validation failed'}
$cert=New-SelfSignedCertificate -Type Custom -Subject 'CN=XeniaRiffmasterLab' -KeyUsage DigitalSignature -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}') -NotAfter (Get-Date).AddYears(1)
& "$($sdk.FullName)\x64\signtool.exe" sign /fd SHA256 /sha1 $cert.Thumbprint $package
if($LASTEXITCODE -ne 0){throw 'Signing failed'}
Export-Certificate -Cert $cert -FilePath xenia-artifacts/XeniaRiffmasterLab.cer | Out-Null
Get-ChildItem xenia-source/build/uwp/xenia-canary-uwp/AppPackages -Filter Microsoft.VCLibs*.appx -Recurse -ErrorAction SilentlyContinue | ForEach-Object {Copy-Item $_.FullName xenia-artifacts/ -Force}
Get-FileHash -Algorithm SHA256 $package | ForEach-Object {"$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))"} | Set-Content xenia-artifacts/SHA256SUMS.txt
Copy-Item LICENSE,THIRD_PARTY_NOTICES.md xenia-artifacts/
Copy-Item docs/INSTALL.md xenia-artifacts/INSTALL.md
@"
Beatles Rock Band Launcher 0.6.0.11, full source build.
Upstream $upstream; patch $env:GITHUB_SHA.
Full WGI buttons + native GameInput whammy/tilt; short polling snapshot; local shared log.
Audio unregister releases global region before waiting on callback; retiring slots cannot be reused.
Audio clients are value initialized; callbacks refreshed under lifetime lock; pump deadline reset on reuse.
UI suppresses guitar analog sensors only in frontend navigation; guest retains full whammy/tilt.
Guitar UI neutral release ignores sensor axes only in the release check; held buttons still block.
Whammy and tilt remain unchanged in guest state. Normal controller release rules unchanged.
Native Xbox account selection maps to persistent offline local saves. Legacy single-profile state is staged and preserved before initialization.
Native-account and story-save console verification is still required for this build.
No game files, songs, charts, proprietary artwork or console captures are included.
"@ | Set-Content xenia-artifacts/BUILD.txt
Get-ChildItem xenia-artifacts -File | Where-Object {$_.Name -ne 'SHA256SUMS.txt'} | ForEach-Object { $hash=Get-FileHash -Algorithm SHA256 $_.FullName; "$($hash.Hash.ToLower())  $($_.Name)" } | Set-Content xenia-artifacts/SHA256SUMS.txt
# Symbols stay private in a separate artifact to support crash analysis.
New-Item xenia-symbols -ItemType Directory -Force | Out-Null
Get-ChildItem xenia-source/build/uwp/bin -Recurse -Filter *.pdb | Where-Object {$_.Name -in @('xeniacanary.pdb','xenia-uwp.pdb','riffinput.pdb')} | ForEach-Object {Copy-Item $_.FullName xenia-symbols/ -Force}
