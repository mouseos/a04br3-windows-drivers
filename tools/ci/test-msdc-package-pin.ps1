$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-msdc-pin-fixture'
if((Test-Path $base) -or (Test-Path 'R:\')){throw 'fixture exists or R busy'}
New-Item -ItemType Directory -Force "$base\ram\a04br3" | Out-Null
try {
 & subst R: "$base\ram";if($LASTEXITCODE){throw 'subst failed'}
 $files=@{'a04-msdc-probe.inf'='9bbf73d73c6dcad9f392e294a2cee7ca23209f80e4d36801e6b6a5781acb7192';'mt8167-msdc.sys'='28115a3ba588d3354e30faca4b37a7de28dcc0b288c7354e2dd3b1ddf3ef416f';'mt8167-msdc.cat'='e88b0514f64cdca47f5b12d6351df51518fbbf47c6253ff469609e96ef3f021c'}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\observe-msdc-storage.c").Replace('L"X:\\a04br3','L"R:\\a04br3')
 foreach($name in $files.Keys){[IO.File]::WriteAllText("R:\a04br3\$name","fixture-$name");$hash=(Get-FileHash -Algorithm SHA256 "R:\a04br3\$name").Hash.ToLowerInvariant();$src=$src.Replace($files[$name],$hash)}
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "$lib\cfgmgr32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $result=& "$base\fixture.exe" --verify-package;if($LASTEXITCODE -ne 0 -or (@($result | Where-Object {$_ -match '=MATCH$'}).Count -ne 3)){throw 'matching package rejected'}
 [IO.File]::WriteAllText('R:\a04br3\a04-msdc-probe.inf','different probe')
 $result=& "$base\fixture.exe" --verify-package;if($LASTEXITCODE -ne 2 -or !($result -contains 'MSDC_PIN_INF=MISMATCH')){throw 'wrong INF accepted'}
 Remove-Item 'R:\a04br3\mt8167-msdc.sys'
 $result=& "$base\fixture.exe" --verify-package;if($LASTEXITCODE -ne 2 -or !($result -contains 'MSDC_PIN_SYS=OPEN_ERROR')){throw 'missing SYS accepted'}
 $result=& "$base\fixture.exe";$rc=$LASTEXITCODE
 if($rc -notin @(0,4) -or !($result -match '^MSDC_PNP_') -or !($result -match '^MSDC_DISK0_') -or !($result -contains 'MSDC_OBSERVATION_READ_ONLY')){throw 'host PnP and disk observation incomplete'}
 Write-Host 'MSDC_PACKAGE_PIN_FIXTURE_PASS (host PnP and disk observations exercised)' 
} finally {& subst R: /d | Out-Null}
