$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$base=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP};$out=Join-Path $base 'mt8167-volume-listener'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -O2 -ffreestanding -fno-builtin -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\mt8167-volume-listener.c') -o "$out\listener.obj"
if($LASTEXITCODE){throw 'volume listener compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\listener.obj" "$lib\kernel32.lib" "/out:$out\mt8167-volume-listener.exe"
if($LASTEXITCODE){throw 'volume listener link failed'}
& llvm-readobj --file-headers "$out\mt8167-volume-listener.exe"
if($LASTEXITCODE){throw 'volume listener inspection failed'}
