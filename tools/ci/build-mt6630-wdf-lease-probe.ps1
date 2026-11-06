$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=Join-Path $tempRoot 'a04br3-wifi-wdf-lease-probe';New-Item -ItemType Directory -Force $out | Out-Null
$src=Join-Path $root 'drivers\network\mt6630\wdf_sdio_probe.c'
foreach ($enabled in @(0,1)) {
& clang "-DA04_WIFI_WDF_LEASE_DIAGNOSTIC=$enabled" --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c $src -o "$out\probe-$enabled.obj"
if($LASTEXITCODE) { throw 'compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib "$out\probe-$enabled.obj" "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\wifi-lease-probe-$enabled.sys"
if($LASTEXITCODE) { throw 'link failed' }
}
Write-Host 'Disabled and opt-in parent lease rejection probes; no INF, MMIO, IRQ or NDIS.'
