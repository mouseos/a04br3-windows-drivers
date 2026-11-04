$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'mt8167-ge8300-provider'
New-Item -ItemType Directory -Force $out | Out-Null
$gpu = Join-Path $root 'drivers\gpu\mt8167'
$boot = if ($env:A04_GE8300_BOOT_TEST -eq '1') { '1' } else { '0' }
$mismatch = if ($env:A04_GE8300_ALLOW_MISMATCH -eq '1') { '1' } else { '0' }
$clear = if ($env:A04_GE8300_CLEAR_TEST -eq '1') { '1' } else { '0' }
$copy = if ($env:A04_GE8300_COPY_TEST -eq '1') { '1' } else { '0' }
$linear = if ($env:A04_GE8300_LINEAR_TEST -eq '1') { '1' } else { '0' }
$service = if ($env:A04_GE8300_COPY_SERVICE -eq '1') { '1' } else { '0' }
$render = if ($env:A04_GE8300_RENDER_EXPERIMENT -eq '1') { '1' } else { '0' }
if ($mismatch -eq '1' -and $boot -ne '1') { throw 'Mismatch opt-in requires the isolated firmware boot test' }
if (($clear -eq '1' -or $copy -eq '1' -or $linear -eq '1') -and ($boot -ne '1' -or $mismatch -ne '1')) { throw 'The isolated GE8300 test requires boot and mismatch opt-ins' }
if ($service -eq '1' -and ($boot -ne '1' -or $mismatch -ne '1' -or $clear -eq '1' -or $copy -eq '1' -or $linear -eq '1')) { throw 'The copy service requires boot/mismatch opt-ins and no diagnostic test' }
if ($render -eq '1' -and ($boot -ne '1' -or $mismatch -ne '1' -or $service -eq '1' -or $clear -eq '1' -or $copy -eq '1' -or $linear -eq '1')) { throw 'Render experiment requires isolated boot/mismatch opt-ins' }
if (([int]$clear + [int]$copy + [int]$linear) -gt 1) { throw 'Choose one GE8300 diagnostic' }
$objects = @()
foreach ($name in @('ge8300_provider','firmware','firmware_runtime','firmware_start','windows_hw','windows_firmware','gpu_mmu','dma_pages','firmware_objects','firmware_queue','render_context','render_pair','render_hwrt','render_target','render_execution','render_freelist','render_stream','transfer_stream','clear_fixture','copy_fixture','linear_copy_fixture','transfer_mapping','windows_transfer_mapping','windows_linear_transfer','windows_linear_context','windows_queue_test')) {
    $obj = Join-Path $out "$name.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_GE8300_BOOT_TEST=$boot" "-DA04_GE8300_ALLOW_MISMATCH=$mismatch" "-DA04_GE8300_CLEAR_TEST=$clear" "-DA04_GE8300_COPY_TEST=$copy" "-DA04_GE8300_LINEAR_TEST=$linear" "-DA04_GE8300_COPY_SERVICE=$service" "-DA04_GE8300_RENDER_EXPERIMENT=$render" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$gpu\compat" "-I$gpu\fwif" -c (Join-Path $gpu "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM GE8300 provider compilation failed: $name" }
    $objects += $obj
}
$package = Join-Path $out 'package'
New-Item -ItemType Directory -Force $package | Out-Null
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$package\mt8167-ge8300.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 provider link failed' }
$firmware = Join-Path $package 'ge8300-mips.fw'
if ($env:A04_GE8300_FIRMWARE) { Copy-Item $env:A04_GE8300_FIRMWARE $firmware -Force } else {
    & curl.exe --fail --location --retry 3 --output $firmware 'https://gitlab.freedesktop.org/imagination/linux-firmware/-/raw/powervr/powervr/rogue_22.67.54.30_v1.fw'
    if ($LASTEXITCODE -ne 0) { throw 'GE8300 firmware download failed' }
}
if ((Get-FileHash $firmware -Algorithm SHA256).Hash -ne 'CFE6F58DB2AD604B1729349629510C5A3434B266C99EA886D2959887BFF4FED8') { throw 'GE8300 firmware SHA-256 mismatch' }
Copy-Item (Join-Path $gpu 'mt8167-ge8300.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'GE8300 provider catalog generation failed' }
Write-Host "GE8300 provider built: boot=$boot mismatch=$mismatch clear=$clear copy=$copy linear=$linear service=$service render=$render. Not a WDDM driver."
