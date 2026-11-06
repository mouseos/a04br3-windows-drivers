$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$temp=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP}
$out=Join-Path $temp 'a04br3-wifi-pnp-uart';New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -O2 -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\mt6630-pnp-uart.c') -o "$out\status.obj"
if($LASTEXITCODE){throw 'Wi-Fi PnP UART compilation failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\status.obj" "$lib\kernel32.lib" "$lib\cfgmgr32.lib" "/out:$out\mt6630-pnp-uart.exe"
if($LASTEXITCODE){throw 'Wi-Fi PnP UART link failed'}
Write-Host 'Read-only PnP status only; session and UART delivery are not proven.'
