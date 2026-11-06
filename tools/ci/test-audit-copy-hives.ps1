$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-audit-copy-hives-fixture'
if(Test-Path $base){throw 'fixture exists'}
if((Test-Path 'R:\') -or (Test-Path 'S:\')){throw 'fixture drive busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\config\TxR","$base\ram\a04br3" | Out-Null
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst target failed'}
 & subst S: "$base\ram";if($LASTEXITCODE){throw 'subst RAM failed'}
 foreach($name in @('SYSTEM','SYSTEM.LOG1','SOFTWARE','SYSTEM{abc}.TM.blf','TxR\{abc}.TxR.0.regtrans-ms')){[IO.File]::WriteAllText("R:\Windows\System32\config\$name","fixture-$name")}
 $src=[IO.File]::ReadAllText("$root\tools\winpe\audit-copy-hives.c").Replace('L"C:\\Windows','L"R:\\Windows').Replace('L"X:\\a04br3','L"S:\\a04br3')
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 4 -or !($lines -contains 'HIVE_COPY_REJECTED_DIRECTORY_EXCLUSION_UNPROVEN')){throw 'unproven directory exclusion accepted'}
 foreach($name in @('SYSTEM','SYSTEM.LOG1','SOFTWARE','SYSTEM{abc}.TM.blf','TxR\{abc}.TxR.0.regtrans-ms')){if((Get-FileHash -Algorithm SHA256 "R:\Windows\System32\config\$name").Hash -ne (Get-FileHash -Algorithm SHA256 "S:\a04br3\audit-system\$name").Hash){throw "copy mismatch: $name"}}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or !($lines -contains 'HIVE_COPY_DEST_EXISTS')){throw 'existing destination accepted'}
 Remove-Item -Recurse -Force 'S:\a04br3\audit-system'
 [IO.File]::WriteAllText('R:\Windows\System32\config\SYSTEM{bad}.txt','unknown')
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 2 -or !($lines -contains 'HIVE_COPY_ENUMERATION_FAILED')){throw 'unknown sidecar accepted'}
 Write-Host 'HIVE_COPY_FIXTURE_PASS (unproven directory exclusion rejected)'
} finally {& subst R: /d | Out-Null;& subst S: /d | Out-Null}
