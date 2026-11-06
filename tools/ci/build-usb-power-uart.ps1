$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$temp=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP}
$out=Join-Path $temp 'a04-usb-power-uart';New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\usb-power-uart.c') -o "$out\usb-power-uart.obj"
if($LASTEXITCODE){throw 'USB observation utility compilation failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\usb-power-uart.obj" "$lib\kernel32.lib" "/out:$out\usb-power-uart.exe"
if($LASTEXITCODE){throw 'USB observation utility link failed'}
