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
$displayScanout = if ($env:A04_GE8300_DISPLAY_SCANOUT_EXPERIMENT -eq '1') { '1' } else { '0' }
$vendorStage = if ($env:A04_GE8300_VENDOR_STAGE -eq '1') { '1' } else { '0' }
$uncachedStage = if ($env:A04_GE8300_VENDOR_UNCACHED_STAGE -eq '1') { '1' } else { '0' }
$vendorIrq = if ($env:A04_GE8300_VENDOR_IRQ -eq '1') { '1' } else { '0' }
$vendorStartDiagnostic = if ($env:A04_GE8300_VENDOR_START_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorRenderCold = if ($env:A04_GE8300_VENDOR_RENDER_COLD -eq '1') { '1' } else { '0' }
$sharedGpuDiagnostic = if ($env:A04_GE8300_SHARED_GPU_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$sharedCommonPage = if ($env:A04_GE8300_SHARED_COMMON_PAGE -eq '1') { '1' } else { '0' }
$sharedFullFrame = if ($env:A04_GE8300_SHARED_FULL_FRAME -eq '1') { '1' } else { '0' }
$wideFrame = if ($env:A04_GE8300_SHARED_WIDE_FRAME -eq '1') { '1' } else { '0' }
if ($wideFrame -eq '1' -and $sharedFullFrame -ne '1') { throw 'Wide frame requires shared full frame support' }

$sharedDisplayCommon = if ($env:A04_GE8300_SHARED_DISPLAY_COMMON -eq '1') { '1' } else { '0' }
if ($sharedFullFrame -eq '1' -and $sharedGpuDiagnostic -ne '1') { throw 'A04_GE8300_SHARED_FULL_FRAME requires A04_GE8300_SHARED_GPU_DIAGNOSTIC' }
if ($sharedCommonPage -eq '1' -and $env:A04_GE8300_SHARED_LEASE -ne '1') { throw 'Shared common page requires shared lease' }
$sharedMmuPublish = if ($env:A04_GE8300_SHARED_MMU_PUBLISH -eq '1') { '1' } else { '0' }
$sharedLease = if ($env:A04_GE8300_SHARED_LEASE -eq '1') { '1' } else { '0' }
$vendorFencedCopy = if ($env:A04_GE8300_VENDOR_FENCED_COPY -eq '1') { '1' } else { '0' }
$vendorCopyService = if ($env:A04_GE8300_VENDOR_COPY_SERVICE -eq '1') { '1' } else { '0' }
$vendorCopyDiagnostic = if ($env:A04_GE8300_VENDOR_COPY_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorClearDiagnostic = if ($env:A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorScreenDiagnostic = if ($env:A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorScreenRepeat = if ($env:A04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($vendorScreenRepeat -eq '1' -and $vendorScreenDiagnostic -ne '1') { throw 'Screen repeat requires screen diagnostic' }
$vendorMultiTileDiagnostic = if ($env:A04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorTileDiagnostic = if ($env:A04_GE8300_VENDOR_TILE_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$vendorColorDiagnostic = if ($env:A04_GE8300_VENDOR_COLOR_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($vendorScreenDiagnostic -eq '1' -and ($vendorTileDiagnostic -ne '1' -or $vendorMultiTileDiagnostic -eq '1' -or $vendorColorDiagnostic -eq '1')) { throw 'Screen diagnostic requires isolated zero tile' }
if ($vendorMultiTileDiagnostic -eq '1' -and $vendorTileDiagnostic -ne '1') { throw 'Multiple tile diagnostic requires offset tile diagnostic' }
if ($vendorTileDiagnostic -eq '1' -and ($vendorClearDiagnostic -ne '1' -or $vendorColorDiagnostic -eq '1')) { throw 'Offset tile requires zero-colour clear diagnostic' }
if ($vendorColorDiagnostic -eq '1' -and $vendorClearDiagnostic -ne '1') { throw 'Vendor colour requires clear diagnostic' }
if ($vendorClearDiagnostic -eq '1' -and $vendorRenderCold -ne '1') { throw 'Vendor clear requires cold render preparation' }
if ($sharedGpuDiagnostic -eq '1' -and $sharedMmuPublish -ne '1') { throw 'Shared GPU diagnostic requires live MMU publication' }
if ($sharedMmuPublish -eq '1' -and $sharedLease -ne '1') { throw 'Live MMU publication requires shared lease' }
if ($sharedLease -eq '1' -and $vendorCopyService -ne '1') { throw 'Shared lease requires vendor copy service' }
if ($vendorFencedCopy -eq '1' -and $vendorCopyService -ne '1') { throw 'Fenced copy requires vendor copy service' }
if ($vendorCopyService -eq '1' -and $vendorCopyDiagnostic -ne '1') { throw 'Vendor copy service requires copy diagnostic resources' }
if ($vendorCopyDiagnostic -eq '1') {
    if ($vendorRenderCold -ne '1' -or $vendorClearDiagnostic -eq '1' -or $vendorScreenDiagnostic -eq '1') { throw 'Copy requires isolated cold render' }
    if (-not $env:A04_GE8300_COPY_USC) { throw 'Copy requires the original native22 USC asset' }
    $asset = [IO.File]::ReadAllBytes($env:A04_GE8300_COPY_USC)
    if ($asset.Length -ne 152 -or (Get-FileHash $env:A04_GE8300_COPY_USC -Algorithm SHA256).Hash -ne '5C68917FAA051238B23674B64C6B137EF7FF75425DBDE9DBC5E1E5A36E02C4A3') { throw 'Copy USC identity mismatch' }
    $items = ($asset | ForEach-Object { '0x{0:x2}' -f $_ }) -join ','
    [IO.File]::WriteAllText((Join-Path $out 'a04_copy_usc_asset.h'),"static const unsigned char a04_copy_usc_asset[152]={$items};`n")
}
if ($vendorRenderCold -eq '1' -and $vendorStartDiagnostic -ne '1') { throw 'Vendor render cold preparation requires the bounded start diagnostic' }
if ($vendorStartDiagnostic -eq '1' -and $vendorIrq -ne '1') { throw 'Vendor start diagnostic requires IRQ' }
if ($vendorIrq -eq '1' -and $uncachedStage -ne '1') { throw 'Vendor IRQ requires uncached staging' }
if ($uncachedStage -eq '1' -and $vendorStage -ne '1') { throw 'Uncached staging requires vendor staging' }
if ($vendorStage -eq '1' -and $boot -eq '1') { throw 'Vendor staging and modern firmware boot are mutually exclusive' }
$mismatch = if ($env:A04_GE8300_ALLOW_MISMATCH -eq '1') { '1' } else { '0' }
$clear = if ($env:A04_GE8300_CLEAR_TEST -eq '1') { '1' } else { '0' }
$copy = if ($env:A04_GE8300_COPY_TEST -eq '1') { '1' } else { '0' }
$linear = if ($env:A04_GE8300_LINEAR_TEST -eq '1') { '1' } else { '0' }
$service = if ($env:A04_GE8300_COPY_SERVICE -eq '1') { '1' } else { '0' }
$sharedTrace = if ($env:A04_GE8300_SHARED_TRACE_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$render = if ($env:A04_GE8300_RENDER_EXPERIMENT -eq '1') { '1' } else { '0' }
$renderDiagnostic = if ($env:A04_GE8300_RENDER_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($sharedTrace -eq '1' -and $boot -ne '1') { throw 'Shared trace diagnostic requires firmware boot opt-in' }
if ($render -eq '1' -and ($boot -ne '1' -or $mismatch -ne '1' -or $service -eq '1' -or $clear -eq '1' -or $copy -eq '1' -or $linear -eq '1')) { throw 'Render experiment requires isolated boot/mismatch opt-ins' }
if ($renderDiagnostic -eq '1' -and ($render -ne '1' -or $service -eq '1' -or $clear -eq '1' -or $copy -eq '1' -or $linear -eq '1')) { throw 'Render diagnostic requires the isolated render experiment' }
if ($mismatch -eq '1' -and $boot -ne '1') { throw 'Mismatch opt-in requires the isolated firmware boot test' }
if (($clear -eq '1' -or $copy -eq '1' -or $linear -eq '1') -and ($boot -ne '1' -or $mismatch -ne '1')) { throw 'The isolated GE8300 test requires boot and mismatch opt-ins' }
if ($service -eq '1' -and ($boot -ne '1' -or $mismatch -ne '1' -or $clear -eq '1' -or $copy -eq '1' -or $linear -eq '1')) { throw 'The copy service requires boot/mismatch opt-ins and no diagnostic test' }
if (([int]$clear + [int]$copy + [int]$linear) -gt 1) { throw 'Choose one GE8300 diagnostic' }
# Keep Device-memory transfers as aligned scalar accesses while optimizing
# ordinary CPU-side queue construction and validation. Debug can request O0.
$optimization = if ($env:A04_GE8300_OPTIMIZE -eq '0') { '-O0' } else { '-O2' }
$codegen = @($optimization,'-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
Write-Host ('GE8300 codegen: ' + ($codegen -join ' '))
$codegen | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $out 'codegen-flags.json')
$objects = @()
$sources = @('ge8300_provider','firmware','firmware_runtime','firmware_start','windows_hw','windows_firmware','gpu_mmu','dma_pages','firmware_objects','firmware_queue','render_context','render_pair','render_hwrt','render_target','render_freelist','render_stream','transfer_stream','clear_fixture','copy_fixture','linear_copy_fixture','transfer_mapping','windows_transfer_mapping','windows_linear_transfer','windows_linear_context','windows_queue_test','vendor_m1_11\stage','vendor_m1_11\windows_stage','vendor_m1_11\windows_start','vendor_m1_11\windows_command','vendor_m1_11\windows_power_acpi','vendor_m1_11\windows_stop')
$sources += @('vendor_m1_11\windows_uncached','vendor_m1_11\windows_stage_uncached','vendor_m1_11\windows_irq','vendor_m1_11\render_resources','vendor_m1_11\windows_render_cold','vendor_m1_11\windows_render_context','vendor_m1_11\windows_render_clear','vendor_m1_11\clear_image','vendor_m1_11\clear_tile','vendor_m1_11\windows_surface','vendor_m1_11\windows_display_scanout','vendor_m1_11\native_copy_state','vendor_m1_11\native_copy_usc','vendor_m1_11\windows_copy_scratch')
if ($sharedLease -eq '1') { $sources += @('vendor_m1_11\windows_shared_scanout','vendor_m1_11\windows_shared_tables') }
if ($sharedMmuPublish -eq '1') { $sources += @('vendor_m1_11\windows_shared_publish') }
if ($render -eq '1') { $sources += @('windows_render_memory','windows_render_diagnostic','render_pds_data','render_context_pds','render_context_static','render_shared_size','render_execution','render_fixed_vdm','render_fixed_ppp','render_fixed_command') }
foreach ($name in $sources) {
    $obj = Join-Path $out "$($name.Replace('\','-')).obj"
    & clang --target=thumbv7-pc-windows-msvc @codegen -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_GE8300_SHARED_DISPLAY_COMMON=$sharedDisplayCommon" "-DA04_GE8300_DISPLAY_SCANOUT_EXPERIMENT=$displayScanout" "-DA04_GE8300_VENDOR_STAGE=$vendorStage" "-DA04_GE8300_VENDOR_UNCACHED_STAGE=$uncachedStage" "-DA04_GE8300_VENDOR_IRQ=$vendorIrq" "-DA04_GE8300_VENDOR_START_DIAGNOSTIC=$vendorStartDiagnostic" "-DA04_GE8300_VENDOR_RENDER_COLD=$vendorRenderCold" "-DA04_GE8300_VENDOR_COPY_DIAGNOSTIC=$vendorCopyDiagnostic" "-DA04_GE8300_SHARED_GPU_DIAGNOSTIC=$sharedGpuDiagnostic" "-DA04_GE8300_SHARED_FULL_FRAME=$sharedFullFrame" "-DA04_GE8300_SHARED_WIDE_FRAME=$wideFrame" "-DA04_GE8300_SHARED_COMMON_PAGE=$sharedCommonPage" "-DA04_GE8300_SHARED_MMU_PUBLISH=$sharedMmuPublish" "-DA04_GE8300_SHARED_LEASE=$sharedLease" "-DA04_GE8300_VENDOR_FENCED_COPY=$vendorFencedCopy" "-DA04_GE8300_VENDOR_COPY_SERVICE=$vendorCopyService" "-DA04_GE8300_VENDOR_CLEAR_DIAGNOSTIC=$vendorClearDiagnostic" "-DA04_GE8300_VENDOR_COLOR_DIAGNOSTIC=$vendorColorDiagnostic" "-DA04_GE8300_VENDOR_TILE_DIAGNOSTIC=$vendorTileDiagnostic" "-DA04_GE8300_VENDOR_MULTI_TILE_DIAGNOSTIC=$vendorMultiTileDiagnostic" "-DA04_GE8300_VENDOR_SCREEN_DIAGNOSTIC=$vendorScreenDiagnostic" "-DA04_GE8300_VENDOR_SCREEN_REPEAT_DIAGNOSTIC=$vendorScreenRepeat" "-DA04_GE8300_BOOT_TEST=$boot" "-DA04_GE8300_ALLOW_MISMATCH=$mismatch" "-DA04_GE8300_CLEAR_TEST=$clear" "-DA04_GE8300_COPY_TEST=$copy" "-DA04_GE8300_LINEAR_TEST=$linear" "-DA04_GE8300_COPY_SERVICE=$service" "-DA04_GE8300_SHARED_TRACE_DIAGNOSTIC=$sharedTrace" "-DA04_GE8300_RENDER_EXPERIMENT=$render" "-DA04_GE8300_RENDER_DIAGNOSTIC=$renderDiagnostic" "-I$out" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$gpu\compat" "-I$gpu\fwif" -c (Join-Path $gpu "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM GE8300 provider compilation failed: $name" }
    $objects += $obj
}
$package = Join-Path $out 'package'
New-Item -ItemType Directory -Force $package | Out-Null
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$package\mt8167-ge8300.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 provider link failed' }
$firmware = Join-Path $package 'ge8300-mips.fw'
if ($vendorStage -eq '1') {
    if (-not $env:A04_GE8300_FIRMWARE) { throw 'Vendor staging requires an explicitly supplied MT8167 firmware' }
    Copy-Item $env:A04_GE8300_FIRMWARE $firmware -Force
    $expectedFirmware = '5C1FDFF55E8E57F8BE0C6C36B94F2167EA53ECE84308068BC79C6B9C37CD7FE1'
} else {
    if ($env:A04_GE8300_FIRMWARE) { Copy-Item $env:A04_GE8300_FIRMWARE $firmware -Force } else {
        & curl.exe --fail --location --retry 3 --output $firmware 'https://gitlab.freedesktop.org/imagination/linux-firmware/-/raw/powervr/powervr/rogue_22.67.54.30_v1.fw'
        if ($LASTEXITCODE -ne 0) { throw 'GE8300 firmware download failed' }
    }
    $expectedFirmware = 'CFE6F58DB2AD604B1729349629510C5A3434B266C99EA886D2959887BFF4FED8'
}
if ((Get-FileHash $firmware -Algorithm SHA256).Hash -ne $expectedFirmware) { throw 'GE8300 firmware SHA-256 mismatch' }
Copy-Item (Join-Path $gpu 'mt8167-ge8300.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'GE8300 provider catalog generation failed' }
Write-Host "GE8300 provider built: boot=$boot mismatch=$mismatch clear=$clear copy=$copy vendorCopy=$vendorCopyDiagnostic linear=$linear service=$service sharedTrace=$sharedTrace render=$render renderDiagnostic=$renderDiagnostic. Not a WDDM driver."
