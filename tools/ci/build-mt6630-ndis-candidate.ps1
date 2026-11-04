$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$out=Join-Path $env:TEMP 'a04br3-ndis-candidate';New-Item -ItemType Directory -Force $out | Out-Null
# WDK 8.1 ndis.h relies on legacy MSVC token-paste expansion.  The private
# build overlay only expands arguments before the same paste for Clang.
$header=[IO.File]::ReadAllText((Join-Path $inc 'km\ndis.h'))
$old='#define NDIS_API_VERSION_AVAILABLE(major,minor) ((((0x ## major) << 8) + (0x ## minor)) >= NDIS_MIN_API)'
$new="#define A04_NDIS_HEX_I(x) 0x ## x`r`n#define A04_NDIS_HEX(x) A04_NDIS_HEX_I(x)`r`n#define NDIS_API_VERSION_AVAILABLE(major,minor) (((A04_NDIS_HEX(major) << 8) + A04_NDIS_HEX(minor)) >= NDIS_MIN_API)"
if(!$header.Contains($old)) { throw 'Unrecognized WDK ndis.h; refusing overlay' }
[IO.File]::WriteAllText((Join-Path $out 'ndis.h'),$header.Replace($old,$new),[Text.Encoding]::ASCII)
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\ndis_candidate.c" -o "$out\ndis.obj"
if($LASTEXITCODE) { throw 'NDIS compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\ndis.obj" "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt6630-ndis-candidate.sys"
if($LASTEXITCODE) { throw 'NDIS link failed' }
Write-Host 'Build-only NDIS candidate: initialize always fails; no INF or hardware access.'

& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\native_scan.c" -o "$out\native_scan.obj"
if($LASTEXITCODE) { throw 'Native 802.11 scan parser compile failed' }
Write-Host 'Native scan request parser compiled; not attached to OID handler.'

& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\native_scan_bridge.c" -o "$out\native_scan_bridge.obj"
if($LASTEXITCODE) { throw 'Native scan bridge compile failed' }

& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\native_nbl.c" -o "$out\native_nbl.obj"
if($LASTEXITCODE) { throw 'Native NBL bridge compile failed' }

& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\native_receive.c" -o "$out\native_receive.obj"
if($LASTEXITCODE) { throw 'Native receive indication compile failed' }
