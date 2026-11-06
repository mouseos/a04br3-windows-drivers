$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-protected-publish-recovery';if(Test-Path $base){throw 'fixture exists'}
if((Test-Path R:\) -or (Test-Path S:\) -or (Test-Path T:\)){throw 'fixture drives busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\config\TxR","$base\target\Windows\System32\drivers","$base\target\Windows\INF","$base\ram\a04br3\update","$base\ram\a04br3\driver-trust","$base\esp\EFI\Boot" | Out-Null
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst target failed'}
 & subst S: "$base\ram";if($LASTEXITCODE){throw 'subst RAM failed'}
 & subst T: "$base\esp";if($LASTEXITCODE){throw 'subst ESP failed'}
 [IO.File]::WriteAllText('T:\EFI\Boot\bootarm.efi','fixture')
 foreach($n in @('SYSTEM','SOFTWARE')){[IO.File]::WriteAllText("R:\Windows\System32\config\$n","original-$n")}
 foreach($n in @('oem33.inf','oem35.inf')){[IO.File]::WriteAllText("R:\Windows\INF\$n","original-$n")}
 foreach($n in @('mt8167-toprgu.sys','bu21025.sys')){[IO.File]::WriteAllText("R:\Windows\System32\drivers\$n","original-$n")}
 $top='R:\Windows\System32\drivers\mt8167-toprgu.sys';$acl=Get-Acl $top;$acl.SetAccessRuleProtection($true,$true);Set-Acl $top $acl
 $beforeAcl=(Get-Acl $top).Sddl;$beforeHash=(Get-FileHash -Algorithm SHA256 $top).Hash
 $files=@{'toprgu'='mt8167-toprgu.sys';'shared'='mt8167-shared.sys';'touch'='bu21025.sys'}
 foreach($k in $files.Keys){$n=$files[$k];[IO.File]::WriteAllText("S:\a04br3\update\$n","candidate-$n");$h=(Get-FileHash -Algorithm SHA256 "S:\a04br3\update\$n").Hash;$bytes=New-Object byte[] 32;for($i=0;$i -lt 32;$i++){$bytes[$i]=[Convert]::ToByte($h.Substring($i*2,2),16)};[IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$k-sys.sha256",$bytes)}
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 foreach($entry in @(@('backup','offline-backup.c','A04_OFFLINE_FIXTURE'),@('stage','offline-stage-drivers.c','A04_STAGE_FIXTURE'),@('publish','offline-publish-drivers.c','A04_PUBLISH_FIXTURE'))){
  $src=[IO.File]::ReadAllText("$root\tools\winpe\$($entry[1])").Replace('L"C:\\','L"R:\\').Replace('L"E:\\','L"T:\\').Replace('L"X:\\','L"S:\\')
  if($entry[0] -eq 'backup'){$src=$src.Replace('if(!volume(L''C'',469762048LL,15147204608LL,L"NTFS")||!volume(L''E'',184549376LL,268435456LL,L"FAT32"))return 0;','if(0)return 0;').Replace('if(!clean_windows_volume())return 0;','if(0)return 0;').Replace('static int flush_volume(WCHAR drive){','static int flush_volume(WCHAR drive){ (void)drive; return 1;')}
  [IO.File]::WriteAllText("$base\$($entry[0]).c",$src)
  & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types "-D$($entry[2])" -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\$($entry[0]).c" -o "$base\$($entry[0]).obj";if($LASTEXITCODE){throw 'compile failed'}
  & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\$($entry[0]).obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\$($entry[0]).exe";if($LASTEXITCODE){throw 'link failed'}
 }
 & "$base\backup.exe";if($LASTEXITCODE){throw 'backup failed'}
 & "$base\backup.exe" --arm;if($LASTEXITCODE){throw 'arm failed'}
 $env:A04_STAGE_FIXTURE_ARMED='1';$env:A04_PUBLISH_FIXTURE_ARMED='1'
 & "$base\stage.exe";if($LASTEXITCODE){throw 'stage failed'}
 $lines=& "$base\publish.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'DRIVER_PUBLISH_SECURITY_CHANGED')){throw 'protected ACL mismatch was not rejected'}
 & "$base\backup.exe" --restore;if($LASTEXITCODE){throw 'protected ACL journal restore failed'}
 if((Get-FileHash -Algorithm SHA256 $top).Hash -ne $beforeHash -or (Get-Acl $top).Sddl -ne $beforeAcl -or !(Get-Acl $top).AreAccessRulesProtected){throw 'protected file content or DACL not restored'}
 if((Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys') -or (Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys.a04restore')){throw 'absent shared SYS or temporary remained'}
 Write-Host 'OFFLINE_PROTECTED_PUBLISH_RECOVERY_PASS (guards and volume flush mocked)'
} finally {
 Remove-Item Env:A04_STAGE_FIXTURE_ARMED -ErrorAction SilentlyContinue;Remove-Item Env:A04_PUBLISH_FIXTURE_ARMED -ErrorAction SilentlyContinue
 & subst R: /d | Out-Null;& subst S: /d | Out-Null;& subst T: /d | Out-Null
}
