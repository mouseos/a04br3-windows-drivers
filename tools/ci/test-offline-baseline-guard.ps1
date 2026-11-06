$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-baseline-fixture';if(Test-Path $base){throw 'fixture exists'}
if(Test-Path R:\){throw 'fixture drive in use'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\drivers","$base\target\Windows\INF" | Out-Null
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 $paths=@{top='R:\Windows\System32\drivers\mt8167-toprgu.sys';touch='R:\Windows\System32\drivers\bu21025.sys';inf33='R:\Windows\INF\oem33.inf';inf35='R:\Windows\INF\oem35.inf'}
 foreach($k in $paths.Keys){[IO.File]::WriteAllText($paths[$k],"fixture-$k")}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-baseline-guard.c").Replace('L"C:\\Windows','L"R:\\Windows')
 foreach($k in $paths.Keys){$hash=(Get-FileHash -Algorithm SHA256 $paths[$k]).Hash;$bytes=(0..31 | ForEach-Object {'0x'+$hash.Substring($_*2,2)}) -join ',';$src=[regex]::Replace($src,"(?<=static const BYTE $k\[32\]=\{)[^}]+",$bytes)}
 $src=$src.Replace('static int volume(void){','static int volume(void){return 1;')
 $src=$src.Replace('if(!GetWindowsDirectoryW(win,128)||lstrcmpiW(win,L"X:\\Windows")||RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_READ,&key)!=ERROR_SUCCESS)', 'if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE",0,KEY_READ,&key)!=ERROR_SUCCESS)')
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -or !($lines -contains 'BASELINE_FILES_MATCH_ONLY_PNP_SIGNATURE_ROLLBACK_UNPROVEN')){throw 'valid baseline rejected'}
 [IO.File]::AppendAllText($paths.touch,'changed');$lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'BASELINE_TOUCH REJECTED')){throw 'modified touch accepted'}
 [IO.File]::WriteAllText($paths.touch,'fixture-touch');[IO.File]::WriteAllText('R:\Windows\System32\drivers\mt8167-shared.sys','unexpected')
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'BASELINE_SHARED_PRESENT_REJECTED')){throw 'unexpected shared file accepted'}
 Remove-Item 'R:\Windows\System32\drivers\mt8167-shared.sys';Remove-Item $paths.inf35
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -match 'BASELINE_INF35 OPEN_ERROR=')){throw 'missing inf accepted'}
 Write-Host 'OFFLINE_BASELINE_GUARD_SYNTHETIC_PASS (volume and WinPE identity mocked)'
} finally {& subst R: /d | Out-Null}
