$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP is required' }
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out=Join-Path $env:RUNNER_TEMP 'a04-map-offline-windows'
New-Item -ItemType Directory -Force $out | Out-Null
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\map-offline-windows.c" -o "$out\map-offline-windows.obj"
if($LASTEXITCODE) { throw 'ARM compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\map-offline-windows.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$out\map-offline-windows.exe"
if($LASTEXITCODE) { throw 'ARM link failed' }
$header= & llvm-readobj --file-headers --coff-imports "$out\map-offline-windows.exe"
if($LASTEXITCODE -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'ARM PE inspection failed' }
$header
Get-FileHash -Algorithm SHA256 "$out\map-offline-windows.exe"
