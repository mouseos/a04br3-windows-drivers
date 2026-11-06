$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-driver-publish-fixture';if(Test-Path $base){throw 'fixture exists'}
if((Test-Path R:\) -or (Test-Path S:\)){throw 'fixture drives busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\drivers","$base\ram\a04br3\driver-trust" | Out-Null
function Bytes([string]$s){$a=New-Object byte[] ($s.Length/2);for($i=0;$i -lt $a.Length;$i++){$a[$i]=[Convert]::ToByte($s.Substring($i*2,2),16)};return ,$a}
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 & subst S: "$base\ram";if($LASTEXITCODE){throw 'subst failed'}
 $files=@{'toprgu'='mt8167-toprgu.sys';'shared'='mt8167-shared.sys';'touch'='bu21025.sys'}
 foreach($k in $files.Keys){$name=$files[$k];[IO.File]::WriteAllText("R:\Windows\System32\drivers\$name.a04restore","new-$name");$h=(Get-FileHash -Algorithm SHA256 "R:\Windows\System32\drivers\$name.a04restore").Hash;[IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$k-sys.sha256",(Bytes $h))}
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys','old-top')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys','old-touch')
 Set-Content -LiteralPath 'R:\Windows\System32\drivers\mt8167-toprgu.sys' -Stream 'fixture' -Value 'original-stream'
 (Get-Item -Force 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Attributes = 'Hidden'
 $attrTop=(Get-Item -Force 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Attributes
 $aclTop=(Get-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Sddl;$aclTouch=(Get-Acl 'R:\Windows\System32\drivers\bu21025.sys').Sddl
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-publish-drivers.c").Replace('L"C:\\','L"R:\\').Replace('L"X:\\','L"S:\\');[IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_PUBLISH_FIXTURE -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj";if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe";if($LASTEXITCODE){throw 'link failed'}
 & "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or [IO.File]::ReadAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys') -ne 'old-top'){throw 'unarmed publish modified target'}
 $env:A04_PUBLISH_FIXTURE_ARMED='1'
 [IO.File]::AppendAllText('R:\Windows\System32\drivers\bu21025.sys.a04restore','tampered')
 & "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or [IO.File]::ReadAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys') -ne 'old-top'){throw 'bad staged file was not rejected before writes'}
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys.a04restore','new-bu21025.sys')
 & "$base\fixture.exe";if($LASTEXITCODE){Write-Host "TOP_ACL_BEFORE=$aclTop";Write-Host "TOP_ACL_AFTER=$((Get-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Sddl)";throw 'publish failed'}
 foreach($name in $files.Values){if([IO.File]::ReadAllText("R:\Windows\System32\drivers\$name") -ne "new-$name" -or (Test-Path "R:\Windows\System32\drivers\$name.a04restore")){throw 'published file mismatch'}}
 if((Get-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Sddl -ne $aclTop -or (Get-Acl 'R:\Windows\System32\drivers\bu21025.sys').Sddl -ne $aclTouch){throw 'target ACL changed'}
 if((Get-Item -Force 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Attributes -ne $attrTop -or (Get-Content -LiteralPath 'R:\Windows\System32\drivers\mt8167-toprgu.sys' -Stream fixture -Raw).Trim() -ne 'original-stream'){throw 'target attribute or alternate stream changed'}
 # A protected descriptor that cannot be preserved exactly must fail closed.
 Remove-Item 'R:\Windows\System32\drivers\mt8167-shared.sys'
 (Get-Item -Force 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Attributes = 'Normal'
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-toprgu.sys','old-top')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys','old-touch')
 $acl=Get-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys';$acl.SetAccessRuleProtection($true,$true);Set-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys' $acl
 $protected=(Get-Acl 'R:\Windows\System32\drivers\mt8167-toprgu.sys').Sddl
 foreach($name in $files.Values){[IO.File]::WriteAllText("R:\Windows\System32\drivers\$name.a04restore","new-$name")}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'DRIVER_PUBLISH_SECURITY_CHANGED') -or !($lines -contains 'DRIVER_PUBLISH_FAILED_JOURNAL_RESTORE_REQUIRED')){throw 'protected descriptor mismatch was not rejected for recovery'}
 Write-Host 'OFFLINE_PUBLISH_DRIVERS_SYNTHETIC_PASS (guards and volume flush mocked; PnP not applied)'
} finally {Remove-Item Env:A04_PUBLISH_FIXTURE_ARMED -ErrorAction SilentlyContinue;Remove-Item Env:A04_PUBLISH_FIXTURE_FAIL -ErrorAction SilentlyContinue;& subst R: /d | Out-Null;& subst S: /d | Out-Null}
