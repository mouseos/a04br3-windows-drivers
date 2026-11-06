$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'offline-backup-fixture'
if(Test-Path $base){throw 'fixture already exists'}
New-Item -ItemType Directory -Path "$base\target","$base\esp" | Out-Null
if ((Test-Path 'R:\') -or (Test-Path 'S:\')) { throw 'fixture drive already in use' }
try {
 & subst.exe R: "$base\target"
 if($LASTEXITCODE){throw 'subst target failed'}
 & subst.exe S: "$base\esp"
 if($LASTEXITCODE){throw 'subst ESP failed'}
 New-Item -ItemType Directory -Force 'R:\Windows\System32\config','R:\Windows\System32\config\TxR','R:\Windows\System32\drivers','R:\Windows\INF','S:\EFI\Boot' | Out-Null
 [IO.File]::WriteAllText('S:\EFI\Boot\bootarm.efi','fixture')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM','original-system')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM.LOG1','original-log')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','original-software')
 [IO.File]::WriteAllText('R:\Windows\System32\config\DRIVERS','original-drivers')
 [IO.File]::WriteAllText('R:\Windows\System32\config\DRIVERS.LOG1','original-drivers-log')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys','original-touch')
 [IO.File]::WriteAllText('R:\Windows\INF\oem33.inf','original-inf33')
 [IO.File]::WriteAllText('R:\Windows\INF\oem35.inf','original-inf35')
 Set-Content -LiteralPath 'R:\Windows\System32\config\SYSTEM' -Stream 'fixture' -Value 'alternate stream'
 (Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes = 'Hidden'
 $acl=(Get-Acl 'R:\Windows\System32\config\SYSTEM').Sddl
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-backup.c")
 $src=$src.Replace('L"C:\\','L"R:\\').Replace('L"E:\\','L"S:\\')
 $src=$src.Replace('if(!volume(L''C'',469762048LL,15147204608LL,L"NTFS")||!volume(L''E'',184549376LL,268435456LL,L"FAT32"))return 0;','if(0)return 0;')
 $src=$src.Replace('if(!clean_windows_volume())return 0;','if(GetEnvironmentVariableW(L"A04_FAKE_DIRTY",0,0))return 0;')
 $src=$src.Replace('static int flush_volume(WCHAR drive){','static int flush_volume(WCHAR drive){ (void)drive; return 1;')
 if($src.Contains('L"C:\\Windows') -or $src.Contains('if(!volume(L''C''')){throw 'fixture path substitution failed'}
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_OFFLINE_FIXTURE -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'fixture compilation failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'fixture link failed'}
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM{fixture}.TM.blf','original-system-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE{fixture}.TMContainer01.regtrans-ms','original-software-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\config\DRIVERS{fixture}.TM.blf','original-drivers-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms','original-txr')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM{bad}.txt','unsupported')
 & "$base\fixture.exe"
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP')){throw 'unsupported sidecar was not rejected'}
 Remove-Item -LiteralPath 'R:\Windows\System32\config\SYSTEM{bad}.txt'
 foreach($i in 1..86){[IO.File]::WriteAllText("R:\Windows\System32\config\SYSTEM{$i}.TM.blf",'extra')}
 & "$base\fixture.exe"
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP')){throw 'sidecar count limit was not enforced'}
 foreach($i in 1..86){Remove-Item -LiteralPath "R:\Windows\System32\config\SYSTEM{$i}.TM.blf"}
 $env:A04_FAKE_DIRTY='1'
 & "$base\fixture.exe"
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP') -or [IO.File]::ReadAllText('R:\Windows\System32\config\SYSTEM') -ne 'original-system'){throw 'dirty target did not reject backup without mutation'}
 Remove-Item Env:A04_FAKE_DIRTY
 $env:A04_FAKE_FULL='1'
 & "$base\fixture.exe"
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP')){throw 'capacity failure did not stop before journal creation'}
 Remove-Item Env:A04_FAKE_FULL
 & "$base\fixture.exe"
 if($LASTEXITCODE){throw 'fixture backup failed'}
 & "$base\fixture.exe" --probe
 if($LASTEXITCODE){throw 'valid recovery journal probe failed'}
 # Derive a valid legacy v3 journal from the v5 fixture, retaining only
 # sidecar types supported by v3 and keeping the original v5 bytes intact.
 $saved="$base\saved-v5";Copy-Item 'S:\A04BACKUP' $saved -Recurse
 $original=[IO.File]::ReadAllBytes('S:\A04BACKUP\READY');$count=[BitConverter]::ToUInt32($original,8)
 $v=[byte[]]::new($original.Length);[Array]::Copy($original,0,$v,0,16+11*328)
 [Array]::Copy([BitConverter]::GetBytes([uint32]3),0,$v,4,4)
 $next=11
 for($i=21;$i -lt $count;$i++){
  $name=[Text.Encoding]::Unicode.GetString($original,16+$i*328+72,256).Split([char]0)[0]
  if($name.StartsWith('DRIVERS{',[StringComparison]::OrdinalIgnoreCase) -or $name.StartsWith('COMPONENTS{',[StringComparison]::OrdinalIgnoreCase)){continue}
  [Array]::Copy($original,16+$i*328,$v,16+$next*328,328)
  Copy-Item (Join-Path $saved ('item{0:D2}.bin' -f $i)) ('S:\A04BACKUP\item{0:D2}.bin' -f $next) -Force
  $next++
 }
 [Array]::Copy([BitConverter]::GetBytes([uint32]$next),0,$v,8,4)
 $digest=[Security.Cryptography.SHA256]::Create();try{$hash=$digest.ComputeHash($v,0,$v.Length-32)}finally{$digest.Dispose()}
 [Array]::Copy($hash,0,$v,$v.Length-32,32);[IO.File]::WriteAllBytes('S:\A04BACKUP\READY',$v)
 & "$base\fixture.exe" --probe;if($LASTEXITCODE){throw 'legacy v3 journal rejected'}
 Remove-Item 'S:\A04BACKUP' -Recurse -Force;Copy-Item $saved 'S:\A04BACKUP' -Recurse
 # Derive a v4 fixture with published INF entries but no new hive entries.
 $v=[byte[]]::new($original.Length);[Array]::Copy($original,0,$v,0,16+13*328)
 [Array]::Copy([BitConverter]::GetBytes([uint32]4),0,$v,4,4);$next=13
 for($i=21;$i -lt $count;$i++){
  $name=[Text.Encoding]::Unicode.GetString($original,16+$i*328+72,256).Split([char]0)[0]
  if($name.StartsWith('DRIVERS{',[StringComparison]::OrdinalIgnoreCase) -or $name.StartsWith('COMPONENTS{',[StringComparison]::OrdinalIgnoreCase)){continue}
  [Array]::Copy($original,16+$i*328,$v,16+$next*328,328)
  Copy-Item (Join-Path $saved ('item{0:D2}.bin' -f $i)) ('S:\A04BACKUP\item{0:D2}.bin' -f $next) -Force;$next++
 }
 [Array]::Copy([BitConverter]::GetBytes([uint32]$next),0,$v,8,4)
 $digest=[Security.Cryptography.SHA256]::Create();try{$hash=$digest.ComputeHash($v,0,$v.Length-32)}finally{$digest.Dispose()}
 [Array]::Copy($hash,0,$v,$v.Length-32,32);[IO.File]::WriteAllBytes('S:\A04BACKUP\READY',$v)
 & "$base\fixture.exe" --probe;if($LASTEXITCODE){throw 'legacy v4 journal rejected'}
 Remove-Item 'S:\A04BACKUP' -Recurse -Force;Copy-Item $saved 'S:\A04BACKUP' -Recurse
 & "$base\fixture.exe" --complete
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP\COMMIT')){throw 'completion permitted while C remains mounted'}
 & "$base\fixture.exe" --unknown
 if(!$LASTEXITCODE){throw 'unknown command accepted'}
 (Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes = 'Normal'
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM','changed')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','changed')
 [IO.File]::WriteAllText('R:\Windows\INF\oem33.inf','changed-inf33')
 [IO.File]::WriteAllText('R:\Windows\INF\oem35.inf','changed-inf35')
 [IO.File]::WriteAllText('R:\Windows\System32\config\DRIVERS','changed-drivers')
 [IO.File]::WriteAllText('R:\Windows\System32\config\DRIVERS{fixture}.TM.blf','changed-drivers-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-shared.sys','new')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM{fixture}.TM.blf','changed-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms','changed-txr')
 $env:A04_FAKE_DIRTY='1'
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE -or [IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'changed' -or !(Test-Path 'S:\A04BACKUP\READY')){throw 'dirty target did not reject replay without mutation'}
 Remove-Item Env:A04_FAKE_DIRTY
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE -or [IO.File]::ReadAllText('R:\Windows\System32\drivers\mt8167-shared.sys') -ne 'new' -or [IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'changed'){throw 'unarmed replay removed newly present target or mutated other files'}
 Remove-Item 'R:\Windows\System32\drivers\mt8167-shared.sys'
 $locked=[IO.File]::Open('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms',[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
 try { & "$base\fixture.exe" --restore; if(!$LASTEXITCODE -or [IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'changed'){throw 'busy TxR did not reject replay before mutation'} } finally { $locked.Dispose() }
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE{new}.TM.blf','new-sidecar')
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE -or [IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'changed'){throw 'unrecorded sidecar did not block replay before mutation'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE{new}.TM.blf') -ne 'new-sidecar'){throw 'unrecorded sidecar modified'}
 Remove-Item -LiteralPath 'R:\Windows\System32\config\SOFTWARE{new}.TM.blf'
 & "$base\fixture.exe" --restore
 if($LASTEXITCODE){throw 'fixture replay failed'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SYSTEM') -ne 'original-system'){throw 'SYSTEM content mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\DRIVERS') -ne 'original-drivers' -or [IO.File]::ReadAllText('R:\Windows\System32\config\DRIVERS{fixture}.TM.blf') -ne 'original-drivers-sidecar'){throw 'DRIVERS hive replay failed'}
 if([IO.File]::ReadAllText('R:\Windows\INF\oem33.inf') -ne 'original-inf33' -or [IO.File]::ReadAllText('R:\Windows\INF\oem35.inf') -ne 'original-inf35'){throw 'published INF replay failed'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'original-software'){throw 'SOFTWARE content mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SYSTEM{fixture}.TM.blf') -ne 'original-system-sidecar'){throw 'SYSTEM sidecar mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms') -ne 'original-txr'){throw 'TxR mismatch'}
 if(Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys'){throw 'absent shared file unexpectedly present'}
 if((Get-Acl 'R:\Windows\System32\config\SYSTEM').Sddl -ne $acl){throw 'ACL mismatch'}
 if(-not ((Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes -band [IO.FileAttributes]::Hidden)){throw 'attributes mismatch'}
 if((Get-Content -LiteralPath 'R:\Windows\System32\config\SYSTEM' -Stream fixture -Raw).Trim() -ne 'alternate stream'){throw 'alternate stream mismatch'}
 & "$base\fixture.exe" --arm
 if($LASTEXITCODE){throw 'fixture arm failed'}
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-shared.sys','armed-new')
 [IO.File]::WriteAllText('R:\Windows\System32\config\COMPONENTS','armed-new-components')
 & "$base\fixture.exe" --restore
 if($LASTEXITCODE -or (Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys') -or (Test-Path 'R:\Windows\System32\config\COMPONENTS')){throw 'armed replay failed to remove new file'}
 foreach($index in 0..24){
   $env:A04_FAIL_AFTER="$index"
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE -ne 98){throw "fixture interruption failed at $index"}
   Remove-Item Env:A04_FAIL_AFTER
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE){throw "fixture resume failed at $index"}
 }
 foreach($index in @(0,2,4,10,11,12,13,15,21,22,23,24)){
   $env:A04_FAIL_TEMP="$index"
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE -ne 98){throw "temporary-file interruption failed at $index"}
   Remove-Item Env:A04_FAIL_TEMP
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE){throw "temporary-file resume failed at $index"}
 }
 [IO.File]::AppendAllText('S:\A04BACKUP\item00.bin','corruption')
 & "$base\fixture.exe" --probe
 if(!$LASTEXITCODE){throw 'corrupted journal probe accepted'}
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','do-not-overwrite')
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE){throw 'corrupted journal accepted'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'do-not-overwrite'){throw 'mutation before validation'}
 Write-Host 'SYNTHETIC_BACKUP_REPLAY_PASS (identity and flush mocked)'
} finally {
 & subst.exe R: /d
 & subst.exe S: /d
}
