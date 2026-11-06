$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'offline-hive-transaction-real-fixture'
if(Test-Path $base){throw 'fixture exists'}
New-Item -ItemType Directory -Path "$base\target","$base\ram","$base\esp" | Out-Null
if((Test-Path 'R:\') -or (Test-Path 'S:\') -or (Test-Path 'T:\')){throw 'fixture drives in use'}
$certs=@()
function From-Hex([string]$value){$result=New-Object byte[] ($value.Length/2);for($i=0;$i -lt $result.Length;$i++){$result[$i]=[Convert]::ToByte($value.Substring(2*$i,2),16)};return ,$result}
try {
 & subst.exe R: "$base\target";if($LASTEXITCODE){throw 'subst target failed'}
 & subst.exe S: "$base\ram";if($LASTEXITCODE){throw 'subst ram failed'}
 & subst.exe T: "$base\esp";if($LASTEXITCODE){throw 'subst esp failed'}
 New-Item -ItemType Directory -Force 'R:\Windows\System32\config','R:\Windows\System32\config\TxR','R:\Windows\System32\drivers','S:\a04br3\driver-trust','T:\EFI\Boot' | Out-Null
 foreach($n in @('mt8167-toprgu.sys','mt8167-shared.sys','bu21025.sys')){[IO.File]::WriteAllText("R:\Windows\System32\drivers\$n","fixture-$n")}
 $pin=@{'toprgu-sys'='mt8167-toprgu.sys';'shared-sys'='mt8167-shared.sys';'touch-sys'='bu21025.sys'}
 foreach($k in $pin.Keys){$hash=(Get-FileHash -Algorithm SHA256 "R:\Windows\System32\drivers\$($pin[$k])").Hash;[IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$k.sha256",(From-Hex $hash))}
 foreach($kind in @('toprgu','shared')){
  $c=New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=A04 Synthetic $kind" -CertStoreLocation 'Cert:\CurrentUser\My';$certs+= $c
  Export-Certificate -Cert $c -FilePath "S:\a04br3\driver-trust\$kind.cer" | Out-Null
  $hash=(Get-FileHash -Algorithm SHA256 "S:\a04br3\driver-trust\$kind.cer").Hash
  [IO.File]::WriteAllBytes("S:\a04br3\driver-trust\$kind-cert.sha256",(From-Hex $hash))
 }
 function Reg([string[]]$arguments){& reg.exe @arguments | Out-Null;if($LASTEXITCODE){throw "registry fixture command failed: $($arguments -join ' ')"}}
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\Select','/v','Current','/t','REG_DWORD','/d','1','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','Start','/t','REG_DWORD','/d','3','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','Type','/t','REG_DWORD','/d','1','/f')
 Reg -arguments @('add','HKLM\SOFTWARE\A04FIXSYS\ControlSet001\Services\Mt8167Toprgu','/v','ImagePath','/t','REG_EXPAND_SZ','/d','\SystemRoot\System32\drivers\mt8167-toprgu.sys','/f')
 Reg -arguments @('save','HKLM\SOFTWARE\A04FIXSYS','R:\Windows\System32\config\SYSTEM','/y');Reg -arguments @('delete','HKLM\SOFTWARE\A04FIXSYS','/f')
 foreach($store in @('Root','TrustedPublisher')){Reg -arguments @('add',"HKLM\SOFTWARE\A04FIXSW\Microsoft\SystemCertificates\$store\Certificates",'/f')}
 Reg -arguments @('save','HKLM\SOFTWARE\A04FIXSW','R:\Windows\System32\config\SOFTWARE','/y');Reg -arguments @('delete','HKLM\SOFTWARE\A04FIXSW','/f')
 [IO.File]::WriteAllText('T:\EFI\Boot\bootarm.efi','fixture')
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-backup.c").Replace('L"C:\\','L"R:\\').Replace('L"E:\\','L"T:\\')
 $src=$src.Replace('if(!volume(L''C'',469762048LL,15147204608LL,L"NTFS")||!volume(L''E'',184549376LL,268435456LL,L"FAT32"))return 0;','if(0)return 0;')
 $src=$src.Replace('if(!clean_windows_volume())return 0;','if(0)return 0;')
 $src=$src.Replace('drives=GetLogicalDrives()','drives=(1u<<4)')
 $src=$src.Replace('!volume(L''E'',184549376LL,268435456LL,L"FAT32")','0')
 $src=$src.Replace('static int flush_volume(WCHAR drive){','static int flush_volume(WCHAR drive){ (void)drive; return 1;')
 [IO.File]::WriteAllText("$base\backup.c",$src)
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\backup.c" -o "$base\backup.obj"
 if($LASTEXITCODE){throw 'backup fixture compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\backup.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" '/out:S:\a04br3\offline-backup.exe'
 if($LASTEXITCODE){throw 'backup fixture link failed'}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-hive-apply.c").Replace('C:\\','R:\\').Replace('X:\\','S:\\')
 [IO.File]::WriteAllText("$base\hive.c",$src)
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_HIVE_FAULT_INJECTION -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\hive.c" -o "$base\hive.obj"
 if($LASTEXITCODE){throw 'hive fixture compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\hive.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "$lib\crypt32.lib" '/out:S:\a04br3\offline-hive-apply.exe'
 if($LASTEXITCODE){throw 'hive fixture link failed'}
 [IO.File]::WriteAllText("$base\release.c",'#include <windows.h>' + "`n" + 'void mainCRTStartup(void){ExitProcess(0);}')
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\release.c" -o "$base\release.obj"
 if($LASTEXITCODE){throw 'release fixture compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\release.obj" "$lib\kernel32.lib" '/out:S:\a04br3\release-offline-volume.exe'
 if($LASTEXITCODE){throw 'release fixture link failed'}
 $batch=(Get-Content -Raw "$root\tools\winpe\apply-offline-hives.cmd").Replace('X:\a04br3','S:\a04br3').Replace('E:\A04BACKUP','T:\A04BACKUP')
 [IO.File]::WriteAllText("$base\transaction.cmd",$batch,[Text.Encoding]::ASCII)
 $before=(Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash
 $beforeSoftware=(Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SOFTWARE').Hash
 $env:A04_HIVE_FAIL_AFTER_SYSTEM='1';$lines=& cmd.exe /c "$base\transaction.cmd";$rc=$LASTEXITCODE;Remove-Item Env:A04_HIVE_FAIL_AFTER_SYSTEM
 if($rc -ne 3 -or !($lines -match 'HIVE_TRANSACTION_ROLLED_BACK')){throw "real rollback failed rc=$rc output=$($lines -join ' | ')"}
 if((Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash -ne $before){throw 'SYSTEM not restored after injected failure'}
 $expectedCommit='A04COMMIT_V1 '+(Get-FileHash -Algorithm SHA256 'T:\A04BACKUP\READY').Hash.ToLowerInvariant()
 if((Get-Content -Raw 'T:\A04BACKUP\COMMIT').Trim() -ne $expectedCommit){throw 'rollback completion marker invalid'}
 & cmd.exe /c 'rmdir /s /q T:\A04BACKUP';if($LASTEXITCODE){throw 'fixture journal cleanup failed'}
 # Exercise a failure after the first certificate has modified SOFTWARE.
 $env:A04_HIVE_FAIL_AFTER_FIRST_CERT='1';$lines=& cmd.exe /c "$base\transaction.cmd";$rc=$LASTEXITCODE;Remove-Item Env:A04_HIVE_FAIL_AFTER_FIRST_CERT
 if($rc -ne 3 -or !($lines -match 'HIVE_TRANSACTION_ROLLED_BACK')){throw "certificate rollback failed rc=$rc output=$($lines -join ' | ')"}
 if((Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash -ne $before -or (Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SOFTWARE').Hash -ne $beforeSoftware){throw 'hives not restored after certificate failure'}
 & cmd.exe /c 'rmdir /s /q T:\A04BACKUP';if($LASTEXITCODE){throw 'fixture journal cleanup failed'}
 # Simulate a process/power interruption after the SYSTEM edit, before the
 # coordinator can run rollback.  The journal and intent survive in the fixture.
 & 'S:\a04br3\offline-backup.exe';if($LASTEXITCODE){throw 'interruption backup failed'}
 & 'S:\a04br3\offline-backup.exe' --arm;if($LASTEXITCODE){throw 'interruption arm failed'}
 $env:A04_HIVE_FAIL_AFTER_SYSTEM='1'; & 'S:\a04br3\offline-hive-apply.exe' --apply;$rc=$LASTEXITCODE;Remove-Item Env:A04_HIVE_FAIL_AFTER_SYSTEM
 if($rc -ne 4){throw 'interruption injection failed'}
 & 'S:\a04br3\offline-backup.exe' --probe;if($LASTEXITCODE){throw 'interrupted journal cannot be found'}
 & 'S:\a04br3\offline-backup.exe' --restore;if($LASTEXITCODE){throw 'interrupted recovery failed'}
 & 'S:\a04br3\offline-backup.exe' --complete;if($LASTEXITCODE){throw 'interrupted recovery completion failed'}
 if((Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash -ne $before){throw 'SYSTEM not restored after interruption'}
 & cmd.exe /c 'rmdir /s /q T:\A04BACKUP';if($LASTEXITCODE){throw 'fixture journal cleanup failed'}
 $lines=& cmd.exe /c "$base\transaction.cmd";$rc=$LASTEXITCODE
 if($rc -ne 0 -or !($lines -match 'HIVE_TRANSACTION_APPLIED_DRIVER_PNP_UNVERIFIED')){throw "real apply failed rc=$rc output=$($lines -join ' | ')"}
 if((Get-Content -Raw 'T:\A04BACKUP\COMMIT').Trim() -ne ('A04COMMIT_V1 '+(Get-FileHash -Algorithm SHA256 'T:\A04BACKUP\READY').Hash.ToLowerInvariant())){throw 'successful completion marker invalid'}
 & 'S:\a04br3\offline-backup.exe' --restore;if(!$LASTEXITCODE){throw 'completed transaction was rolled back implicitly'}
 Reg -arguments @('load','HKLM\A04VERIFY','R:\Windows\System32\config\SYSTEM')
 $q=& reg.exe query 'HKLM\A04VERIFY\ControlSet001\Services\Mt8167Toprgu' /v Start
 if($LASTEXITCODE -or ($q -join ' ') -notmatch '0x0'){throw 'service value mismatch'}
 Reg -arguments @('unload','HKLM\A04VERIFY')
 Reg -arguments @('load','HKLM\A04VERIFY','R:\Windows\System32\config\SOFTWARE')
 foreach($store in @('Root','TrustedPublisher')){foreach($c in $certs){& reg.exe query "HKLM\A04VERIFY\Microsoft\SystemCertificates\$store\Certificates\$($c.Thumbprint)" /v Blob | Out-Null;if($LASTEXITCODE){throw 'certificate blob missing'}}}
 Reg -arguments @('unload','HKLM\A04VERIFY')
 Write-Host 'OFFLINE_HIVE_REAL_TRANSACTION_SYNTHETIC_PASS'

} finally {
 $ErrorActionPreference='Continue'
 Remove-Item Env:A04_FIXTURE_JOURNAL -ErrorAction SilentlyContinue
 foreach($name in @('A04VERIFY','A04TXSYS','A04TXSW')){& cmd.exe /c "reg unload HKLM\$name >nul 2>nul" | Out-Null}
 foreach($name in @('A04FIXSYS','A04FIXSW')){& cmd.exe /c "reg delete HKLM\SOFTWARE\$name /f >nul 2>nul" | Out-Null}
 foreach($c in $certs){Remove-Item -LiteralPath ("Cert:\CurrentUser\My\"+$c.Thumbprint) -Force -ErrorAction SilentlyContinue}
 & subst.exe R: /d; & subst.exe S: /d; & subst.exe T: /d
}
$global:LASTEXITCODE=0
