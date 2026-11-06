$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP is required' }
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out=Join-Path $env:RUNNER_TEMP 'a04-audit-copy-hives'
New-Item -ItemType Directory -Force $out | Out-Null
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\audit-copy-hives.c" -o "$out\audit-copy-hives.obj"
if($LASTEXITCODE) { throw 'ARM compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\audit-copy-hives.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$out\audit-copy-hives.exe"
if($LASTEXITCODE) { throw 'ARM link failed' }
$header= & llvm-readobj --file-headers --coff-imports "$out\audit-copy-hives.exe"
if($LASTEXITCODE -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'ARM PE inspection failed' }
$header
Get-FileHash -Algorithm SHA256 "$out\audit-copy-hives.exe"
