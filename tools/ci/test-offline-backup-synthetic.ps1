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
 New-Item -ItemType Directory -Force 'R:\Windows\System32\config','R:\Windows\System32\drivers','S:\EFI\Boot' | Out-Null
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
 $src=$src.Replace('if(!clean_windows_volume())return 0;','if(0)return 0;')
 $src=$src.Replace('static int flush_volume(WCHAR drive){','static int flush_volume(WCHAR drive){ (void)drive; return 1;')
 if($src.Contains('L"C:\\Windows') -or $src.Contains('if(!volume(L''C''')){throw 'fixture path substitution failed'}
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -DA04_OFFLINE_FIXTURE -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'fixture compilation failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'fixture link failed'}
 & "$base\fixture.exe"
 if($LASTEXITCODE){throw 'fixture backup failed'}
 (Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes = 'Normal'
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM','changed')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','changed')
 [IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-shared.sys','new')
 & "$base\fixture.exe" --restore
 if($LASTEXITCODE){throw 'fixture replay failed'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SYSTEM') -ne 'original-system'){throw 'SYSTEM content mismatch'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'original-software'){throw 'SOFTWARE content mismatch'}
 if(Test-Path 'R:\Windows\System32\drivers\mt8167-shared.sys'){throw 'new file not removed'}
 if((Get-Acl 'R:\Windows\System32\config\SYSTEM').Sddl -ne $acl){throw 'ACL mismatch'}
 if(-not ((Get-Item -Force 'R:\Windows\System32\config\SYSTEM').Attributes -band [IO.FileAttributes]::Hidden)){throw 'attributes mismatch'}
 if((Get-Content -LiteralPath 'R:\Windows\System32\config\SYSTEM' -Stream fixture -Raw).Trim() -ne 'alternate stream'){throw 'alternate stream mismatch'}
 & "$base\fixture.exe" --restore
 if($LASTEXITCODE){throw 'fixture repeat replay failed'}
 foreach($index in 0..10){
   $env:A04_FAIL_AFTER="$index"
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE -ne 98){throw "fixture interruption failed at $index"}
   Remove-Item Env:A04_FAIL_AFTER
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE){throw "fixture resume failed at $index"}
 }
 foreach($index in @(0,2,4,10)){
   $env:A04_FAIL_TEMP="$index"
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE -ne 98){throw "temporary-file interruption failed at $index"}
   Remove-Item Env:A04_FAIL_TEMP
   & "$base\fixture.exe" --restore
   if($LASTEXITCODE){throw "temporary-file resume failed at $index"}
 }
 [IO.File]::AppendAllText('S:\A04BACKUP\item00.bin','corruption')
 [IO.File]::WriteAllText('R:\Windows\System32\config\SOFTWARE','do-not-overwrite')
 & "$base\fixture.exe" --restore
 if(!$LASTEXITCODE){throw 'corrupted journal accepted'}
 if([IO.File]::ReadAllText('R:\Windows\System32\config\SOFTWARE') -ne 'do-not-overwrite'){throw 'mutation before validation'}
 Write-Host 'SYNTHETIC_BACKUP_REPLAY_PASS (identity and flush mocked)'
} finally {
 & subst.exe R: /d
 & subst.exe S: /d
}
