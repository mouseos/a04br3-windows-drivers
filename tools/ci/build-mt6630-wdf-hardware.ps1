# ARM32 link check for the guarded hardware path. Configuration remains off.
$ErrorActionPreference='Stop'
$env:A04_WIFI_IRQ_DIAGNOSTIC='1'
$env:A04_WIFI_SESSION_DIAGNOSTIC='1'
& "$PSScriptRoot\build-mt6630-core.ps1"
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"; $inc=Join-Path $kit 'Include'
$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'; $km=Join-Path $kit 'Lib\winv6.3\km\arm'
$core=Join-Path $env:TEMP 'a04br3-mt6630-core'
$out=Join-Path $env:RUNNER_TEMP 'a04br3-wifi-wdf-hardware'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_WIFI_WDF_LEASE_DIAGNOSTIC=1 -DA04_WIFI_WDF_HARDWARE_DIAGNOSTIC=1 -DA04_WIFI_SESSION_DIAGNOSTIC=1 -DA04_WIFI_IRQ_DIAGNOSTIC=1 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c "$root\drivers\network\mt6630\wdf_sdio_probe.c" -o "$out\probe.obj"
if ($LASTEXITCODE) { throw 'WDF hardware path compile failed' }
$objects=Get-ChildItem "$core\*.obj" | ForEach-Object { $_.FullName }
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref "$out\probe.obj" $objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\wifi-hardware-config-disabled.sys"
if ($LASTEXITCODE) { throw 'WDF hardware path link failed' }
Write-Host 'ARM32 hardware path compiled and linked; verified configuration remains disabled.'
