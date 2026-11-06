$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-driver-stage-fixture';if(Test-Path $base){throw 'fixture exists'}
if((Test-Path R:\) -or (Test-Path S:\)){throw 'fixture drives busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\drivers","$base\ram\a04br3\update","$base\ram\a04br3\driver-trust" | Out-Null
function Bytes([string]$s){$a=New-Object byte[] ($s.Length/2);for($i=0;$i -lt $a.Length;$i++){$a[$i]=[Convert]::ToByte($s.Substring($i*2,2),16)};return ,$a}
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 & subst S: "$base\ram";if($LASTEXITCODE){throw 'subst failed'}
 $files=@{'toprgu'='mt8167-toprgu.sys';'shared'='mt8167-shared.sys';'touch'='bu21025.sys'}
 foreach($k in $files.Keys){$name=$files[$k];[IO.File]::WriteAllText("S:\a04br3\update\$name","new-$name");$h=(Get-FileHash -Algorithm SHA256 "S:\a04br3\update\$name").Hash;[IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$k-sys.sha256",(Bytes $h))}
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys','old-top')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys','old-touch')
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-stage-drivers.c").Replace('L"C:\\','L"R:\\').Replace('L"X:\\','L"S:\\');[IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_STAGE_FIXTURE -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj";if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe";if($LASTEXITCODE){throw 'link failed'}
 & "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or (Test-Path 'R:\Windows\System32\drivers\mt8167-toprgu.sys.a04restore')){throw 'unarmed stage wrote files'}
 $env:A04_STAGE_FIXTURE_ARMED='1'
 & "$base\fixture.exe";if($LASTEXITCODE){throw 'valid stage failed'}
 foreach($name in $files.Values){if((Get-FileHash -Algorithm SHA256 "R:\Windows\System32\drivers\$name.a04restore").Hash -ne (Get-FileHash -Algorithm SHA256 "S:\a04br3\update\$name").Hash){throw 'staged hash mismatch'}}
 if([IO.File]::ReadAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys') -ne 'old-top' -or [IO.File]::ReadAllText('R:\Windows\System32\drivers\bu21025.sys') -ne 'old-touch' -or (Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys')){throw 'stage altered target drivers'}
 & "$base\fixture.exe";if($LASTEXITCODE -ne 2){throw 'existing temporary files accepted'}
 foreach($name in $files.Values){Remove-Item "R:\Windows\System32\drivers\$name.a04restore"}
 $env:A04_STAGE_FIXTURE_FAIL='0';& "$base\fixture.exe";if($LASTEXITCODE -ne 98 -or !(Test-Path 'R:\Windows\System32\drivers\mt8167-toprgu.sys.a04restore')){throw 'interruption fixture failed'}
 Remove-Item Env:A04_STAGE_FIXTURE_FAIL;Remove-Item 'R:\Windows\System32\drivers\mt8167-toprgu.sys.a04restore'
 [IO.File]::AppendAllText('S:\a04br3\update\mt8167-toprgu.sys','tampered');& "$base\fixture.exe";if($LASTEXITCODE -ne 3){throw 'modified candidate accepted'}
 if([IO.File]::ReadAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys') -ne 'old-top'){throw 'target changed on candidate mismatch'}
 Write-Host 'OFFLINE_STAGE_DRIVERS_SYNTHETIC_PASS (guards mocked; no target replacement)'
} finally {Remove-Item Env:A04_STAGE_FIXTURE_ARMED -ErrorAction SilentlyContinue;Remove-Item Env:A04_STAGE_FIXTURE_FAIL -ErrorAction SilentlyContinue;& subst R: /d | Out-Null;& subst S: /d | Out-Null}
