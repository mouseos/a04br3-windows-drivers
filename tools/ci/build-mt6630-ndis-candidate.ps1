$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$out=Join-Path $env:TEMP 'a04br3-ndis-candidate';New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\ndis_candidate.c" -o "$out\ndis.obj"
if($LASTEXITCODE) { throw 'NDIS compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\ndis.obj" "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt6630-ndis-candidate.sys"
if($LASTEXITCODE) { throw 'NDIS link failed' }
Write-Host 'Build-only NDIS candidate: initialize always fails; no INF or hardware access.'
