$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$base=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP};$out=Join-Path $base 'a04-fourcpu-load'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\cpu\fourcpu-load.c') -o "$out\fourcpu-load.obj"
if($LASTEXITCODE){throw 'ARM32 compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\fourcpu-load.obj" "$lib\kernel32.lib" "/out:$out\fourcpu-load.exe"
if($LASTEXITCODE){throw 'ARM32 link failed'}
& llvm-readobj --file-headers "$out\fourcpu-load.exe"
if($LASTEXITCODE){throw 'inspection failed'}
Write-Host 'Diagnostic only: 60 seconds of CPU execution does not prove long-term or sleep stability.'
