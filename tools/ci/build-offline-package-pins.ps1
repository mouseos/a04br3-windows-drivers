$ErrorActionPreference='Stop'
if(!$env:RUNNER_TEMP){throw 'RUNNER_TEMP required'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $env:RUNNER_TEMP 'a04-offline-package-pins';New-Item -ItemType Directory -Force $out | Out-Null
$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
& clang --target=thumbv7-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\offline-package-pins.c" -o "$out\pins.obj"
if($LASTEXITCODE){throw 'ARM compilation failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\pins.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$out\offline-package-pins.exe"
if($LASTEXITCODE){throw 'ARM link failed'}
$header=& llvm-readobj --file-headers --coff-imports "$out\offline-package-pins.exe"
if($LASTEXITCODE -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARM PE inspection failed'}
Get-FileHash -Algorithm SHA256 "$out\offline-package-pins.exe"
