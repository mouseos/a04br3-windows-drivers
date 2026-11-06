# Standalone, default-off GCE PDO resource owner. Does not install or submit DMA.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$wdfInc=Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib=Join-Path $kit 'Lib\winv6.3\km\arm'
if(!$env:RUNNER_TEMP){throw 'RUNNER_TEMP is required'}
$out=Join-Path $env:RUNNER_TEMP 'mt8167-gce-provider'
$package=Join-Path $out 'package'
New-Item -ItemType Directory -Force $package | Out-Null
$src=Join-Path $root 'drivers\gpu\mt8167\gce'
$parentClock=if($env:A04_GCE_PARENT_CLOCK_INTERFACE -eq '1'){'1'}else{'0'}
$displayLease=if($env:A04_GCE_DISPLAY_LEASE_INTERFACE -eq '1'){'1'}else{'0'}
$irqAttach=if($env:A04_GCE_IRQ_ATTACH -eq '1'){'1'}else{'0'}
$handoffObserve=if($env:A04_GCE_HANDOFF_OBSERVE -eq '1'){'1'}else{'0'}
$obj=Join-Path $out 'mt8167_gce.obj'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_GCE_HANDOFF_OBSERVE=$handoffObserve" "-DA04_GCE_IRQ_ATTACH=$irqAttach" "-DA04_GCE_DISPLAY_LEASE_INTERFACE=$displayLease" "-DA04_GCE_PARENT_CLOCK_INTERFACE=$parentClock" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $src 'mt8167_gce.c') -o $obj
if($LASTEXITCODE){throw 'GCE ARM source compilation failed'}
$submitObj=Join-Path $out 'gce_submit.obj'
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $src 'gce_submit.c') -o $submitObj
if($LASTEXITCODE){throw 'GCE command core compilation failed'}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib $obj $submitObj (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$package\mt8167-gce.sys"
if($LASTEXITCODE){throw 'GCE ARM driver link failed'}
Copy-Item (Join-Path $src 'mt8167-gce.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE){throw 'GCE catalog generation failed'}
Write-Host 'GCE resource owner only: no commands, IRQ, clock or display switching.'
