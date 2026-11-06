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
 New-Item -ItemType Directory -Force 'R:\Windows\System32\config','R:\Windows\System32\config\TxR','R:\Windows\System32\drivers','S:\EFI\Boot' | Out-Null
 [IO.File]::WriteAllText('S:\EFI\Boot\bootarm.efi','fixture')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM','original-system')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM.LOG1','original-log')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','original-software')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\bu21025.sys','original-touch')
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
 & "$base\fixture.exe" --complete
 if(!$LASTEXITCODE -or (Test-Path 'S:\A04BACKUP\COMMIT')){throw 'completion permitted while C remains mounted'}
 & "$base\fixture.exe" --unknown
 if(!$LASTEXITCODE){throw 'unknown command accepted'}
 (Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes = 'Normal'
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM','changed')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','changed')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-shared.sys','new')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM{fixture}.TM.blf','changed-sidecar')
 [IO.File]::WriteAllText('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms','changed-txr')
 $env:A04_FAKE_DIRTY='1'
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE -or [IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'changed' -or !(Test-Path 'S:\A04BACKUP\READY')){throw 'dirty target did not reject replay without mutation'}
 Remove-Item Env:A04_FAKE_DIRTY
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
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'original-software'){throw 'SOFTWARE content mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SYSTEM{fixture}.TM.blf') -ne 'original-system-sidecar'){throw 'SYSTEM sidecar mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\TxR\{fixture}.TxR.0.regtrans-ms') -ne 'original-txr'){throw 'TxR mismatch'}
 if(Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys'){throw 'new file not removed'}
 if((Get-Acl 'R:\Windows\System32\config\SYSTEM').Sddl -ne $acl){throw 'ACL mismatch'}
 if(-not ((Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes -band [IO.FileAttributes]::Hidden)){throw 'attributes mismatch'}
 if((Get-Content -LiteralPath 'R:\Windows\System32\config\SYSTEM' -Stream fixture -Raw).Trim() -ne 'alternate stream'){throw 'alternate stream mismatch'}
 & "$base\fixture.exe" --restore
 if($LASTEXITCODE){throw 'fixture repeat replay failed'}
 foreach($index in 0..13){
   $env:A04_FAIL_AFTER="$index"
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE -ne 98){throw "fixture interruption failed at $index"}
   Remove-Item Env:A04_FAIL_AFTER
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE){throw "fixture resume failed at $index"}
 }
 foreach($index in @(0,2,4,10,11,12,13)){
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
