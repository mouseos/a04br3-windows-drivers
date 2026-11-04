$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$include = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$source = Join-Path $PSScriptRoot '..\..\drivers\storage\mt8167-msdc\miniport.c'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$output = Join-Path $tempRoot 'a04br3-wdk81-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$object = Join-Path $output 'miniport.obj'
$pioObject = Join-Path $output 'pio.obj'
$pioSource = Join-Path $PSScriptRoot '..\..\drivers\storage\mt8167-msdc\pio.c'
$driver = Join-Path $output 'miniport.sys'
$extraObjects = @()
$probeDefines = @()
if ($env:A04_SYNC_PIO_MULTIREAD -eq '1') {
    if ($env:A04_READONLY_DISK_PROBE -ne '1' -or $env:A04_DMA_PROBE -eq '1' -or $env:A04_DMA_DISK_PROBE -eq '1' -or $env:A04_PASSIVE_IO -eq '1' -or $env:A04_ENABLE_WRITES -eq '1' -or $env:A04_VIRTUAL_BOUNCE_EXPERIMENT -eq '1' -or $env:A04_DMA_MULTIBLOCK_EXPERIMENT -eq '1' -or $env:A04_BOOT_MULTIBLOCK_EXPERIMENT -eq '1' -or $env:A04_SCRATCH_MULTIBLOCK_EXPERIMENT -eq '1') { throw 'Synchronous PIO requires a physical read-only non-worker probe' }
    $probeDefines += '-DA04_SYNC_PIO_MULTIREAD'
}
if ($env:A04_DMA_MULTIBLOCK_EXPERIMENT -eq '1') {
    if ($env:A04_DMA_PROBE -ne '1' -or $env:A04_DMA_DISK_PROBE -ne '1' -or $env:A04_READONLY_DISK_PROBE -ne '1' -or $env:A04_VIRTUAL_BOUNCE_EXPERIMENT -ne '1' -or $env:A04_ENABLE_WRITES -eq '1' -or $env:A04_DMA_WRITE_TEST -eq '1' -or $env:A04_BOOT_EXPERIMENT -eq '1') { throw 'Multiblock experiment requires a read-only non-boot virtual-bounce DMA disk probe' }
    $probeDefines += '-DA04_DMA_MULTIBLOCK_EXPERIMENT=1'
}
if ($env:A04_BOOT_MULTIBLOCK_EXPERIMENT -eq '1') {
    if ($env:A04_BOOT_PASSIVE_EXPERIMENT -ne '1' -or $env:A04_DMA_PROBE -ne '1' -or $env:A04_READONLY_DISK_PROBE -ne '1' -or $env:A04_DMA_MULTIBLOCK_EXPERIMENT -eq '1' -or $env:A04_DMA_WRITE_TEST -eq '1') { throw 'Boot multiblock requires the isolated passive boot configuration and excludes the read-only experiment' }
    $probeDefines += '-DA04_BOOT_MULTIBLOCK_EXPERIMENT=1'
}
if ($env:A04_SCRATCH_MULTIBLOCK_EXPERIMENT -eq '1') {
    if ($env:A04_SCRATCH_IO -ne '1' -or $env:A04_PASSIVE_IO -ne '1' -or $env:A04_DMA_PROBE -ne '1' -or $env:A04_DMA_DISK_PROBE -ne '1' -or $env:A04_VIRTUAL_BOUNCE_EXPERIMENT -ne '1' -or $env:A04_SERIAL_QUEUE_EXPERIMENT -ne '1' -or $env:A04_ISOLATED_REQUEST_EXPERIMENT -ne '1' -or $env:A04_BOOT_EXPERIMENT -eq '1' -or $env:A04_DMA_MULTIBLOCK_EXPERIMENT -eq '1' -or $env:A04_BOOT_MULTIBLOCK_EXPERIMENT -eq '1' -or $env:A04_DMA_WRITE_TEST -eq '1') { throw 'Scratch multiblock requires isolated bounded scratch settings' }
    $probeDefines += '-DA04_SCRATCH_MULTIBLOCK_EXPERIMENT=1'
}
if ($env:A04_DMA_POLL_TRACE -eq '1') {
    if ($env:A04_DMA_PROBE -ne '1') { throw 'DMA poll tracing requires A04_DMA_PROBE=1' }
    $probeDefines += '-DA04_DMA_POLL_TRACE'
}
if ($env:A04_PASSIVE_IO -eq '1') {
    if ($env:A04_READONLY_DISK_PROBE -ne '1' -or ($env:A04_BOOT_EXPERIMENT -eq '1' -and $env:A04_BOOT_PASSIVE_EXPERIMENT -ne '1') -or ($env:A04_ENABLE_WRITES -eq '1' -and $env:A04_SCRATCH_IO -ne '1' -and $env:A04_BOOT_PASSIVE_EXPERIMENT -ne '1')) { throw 'Passive I/O requires an explicit diagnostic configuration' }
    $probeDefines += '-DA04_PASSIVE_IO'
}
if ($env:A04_SCRATCH_IO -eq '1') {
    if ($env:A04_PASSIVE_IO -ne '1' -or $env:A04_ENABLE_WRITES -ne '1' -or $env:A04_BOOT_EXPERIMENT -eq '1' -or $env:A04_SCRATCH_LBA -notmatch '^\d+$') { throw 'Scratch I/O requires passive guarded writes, a decimal LBA and a non-boot package' }
    $scratchLba = [uint64]$env:A04_SCRATCH_LBA
    if ($scratchLba -gt 4294967288) { throw 'Scratch range exceeds 32-bit sectors' }
    $probeDefines += '-DA04_SCRATCH_IO'
    $probeDefines += ('-DA04_SCRATCH_LBA=' + $scratchLba)
}
if ($env:A04_SERIAL_QUEUE_EXPERIMENT -eq '1') {
    if ($env:A04_SCRATCH_IO -ne '1' -and $env:A04_BOOT_PASSIVE_EXPERIMENT -ne '1') { throw 'Serial queue experiment requires an explicit diagnostic configuration' }
    $probeDefines += '-DA04_SERIAL_QUEUE_EXPERIMENT'
}
if ($env:A04_ISOLATED_REQUEST_EXPERIMENT -eq '1') {
    if ($env:A04_SCRATCH_IO -ne '1' -and $env:A04_BOOT_PASSIVE_EXPERIMENT -ne '1') { throw 'Isolated requests require an explicit diagnostic configuration' }
    $probeDefines += '-DA04_ISOLATED_REQUEST_EXPERIMENT'
}
if ($env:A04_ISOLATED_RESET_TEST -eq '1') {
    if ($env:A04_ISOLATED_REQUEST_EXPERIMENT -ne '1') { throw 'Reset test requires isolated requests' }
    $probeDefines += '-DA04_ISOLATED_RESET_TEST'
}
if ($env:A04_BOOT_PASSIVE_EXPERIMENT -eq '1') {
    if ($env:A04_BOOT_EXPERIMENT -ne '1' -or $env:A04_PASSIVE_IO -ne '1' -or $env:A04_ENABLE_WRITES -ne '1' -or $env:A04_DMA_DISK_PROBE -ne '1' -or $env:A04_VIRTUAL_BOUNCE_EXPERIMENT -ne '1' -or $env:A04_SERIAL_QUEUE_EXPERIMENT -ne '1' -or $env:A04_ISOLATED_REQUEST_EXPERIMENT -ne '1' -or $env:A04_SCRATCH_IO -eq '1' -or $env:A04_ISOLATED_RESET_TEST -eq '1') { throw 'Passive boot experiment requires guarded isolated boot settings' }
    $probeDefines += '-DA04_BOOT_PASSIVE_EXPERIMENT'
}
if ($env:A04_EMMC_IDENTITY_HANDOFF -eq '1') {
    if ($env:A04_READONLY_DISK_PROBE -ne '1') { throw 'eMMC identity handoff requires the disk probe' }
    $probeDefines += '-DA04_EMMC_IDENTITY_HANDOFF'
}
if ($env:A04_DMA_WRITE_TEST -eq '1') {
    if ($env:A04_DMA_PROBE -ne '1' -or $env:A04_DMA_DISK_PROBE -eq '1' -or $env:A04_DMA_TEST_LBA -notmatch '^\d+$') { throw 'DMA write test requires DMA probe, no disk probe, and a decimal A04_DMA_TEST_LBA' }
}
if ($env:A04_VIRTUAL_BOUNCE_EXPERIMENT -eq '1' -and $env:A04_DMA_DISK_PROBE -ne '1') { throw 'A04_VIRTUAL_BOUNCE_EXPERIMENT requires A04_DMA_DISK_PROBE=1' }
if ($env:A04_ENABLE_WRITES -eq '1' -and $env:A04_DMA_DISK_PROBE -ne '1') { throw 'A04_ENABLE_WRITES requires A04_DMA_DISK_PROBE=1' }
if ($env:A04_BOOT_EXPERIMENT -eq '1' -and ($env:A04_ENABLE_WRITES -ne '1' -or $env:A04_VIRTUAL_BOUNCE_EXPERIMENT -ne '1')) { throw 'Boot experiment requires guarded writes and the diagnostic virtual path' }
if ($env:A04_DMA_DISK_PROBE -eq '1' -and $env:A04_DMA_PROBE -ne '1') { throw 'A04_DMA_DISK_PROBE requires A04_DMA_PROBE=1' }
if ($env:A04_DMA_PROBE -eq '1' -and $env:A04_READONLY_DISK_PROBE -ne '1') { throw 'A04_DMA_PROBE requires A04_READONLY_DISK_PROBE=1' }
if ($env:A04_READONLY_DISK_PROBE -eq '1') {
    $probeDefines += '-DA04_READONLY_DISK_PROBE'
    $sources = @('cdb', 'scsi_capacity', 'scsi')
    if ($env:A04_SYNC_PIO_MULTIREAD -eq '1') { $sources += @('sync_pio', 'resume_check', 'resume_capture', 'board_owner', 'cold_init', 'pinmux') }
    if ($env:A04_DMA_PROBE -eq '1') { $probeDefines += '-DA04_DMA_PROBE'; $sources += 'dma' }
    if ($env:A04_DMA_DISK_PROBE -eq '1') { $probeDefines += '-DA04_DMA_DISK_PROBE' }
    if ($env:A04_ENABLE_WRITES -eq '1') { $probeDefines += '-DA04_ENABLE_WRITES'; $sources += 'gpt_guard' }
    if ($env:A04_DMA_WRITE_TEST -eq '1') { $probeDefines += '-DA04_DMA_WRITE_TEST'; $probeDefines += ('-DA04_DMA_TEST_LBA=' + $env:A04_DMA_TEST_LBA) }
    if ($env:A04_VIRTUAL_BOUNCE_EXPERIMENT -eq '1') { $probeDefines += '-DA04_VIRTUAL_BOUNCE_EXPERIMENT' }
    foreach ($name in $sources) {
        $obj = Join-Path $output "$name.obj"
        $src = Join-Path $PSScriptRoot "..\..\drivers\storage\mt8167-msdc\$name.c"
        & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $src -o $obj
        if ($LASTEXITCODE -ne 0) { throw "ARM32 $name compilation failed" }
        $extraObjects += $obj
    }
}
# The old WDK headers produce many diagnostics with current LLVM; compile this
# interface probe separately from production builds and retain the compiler exit code.
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$include\km" "-I$include\shared" "-I$include\um" @probeDefines -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw 'ARM32 interface probe compilation failed' }
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $pioSource -o $pioObject
if ($LASTEXITCODE -ne 0) { throw 'ARM32 PIO compilation failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib $object $pioObject @extraObjects (Join-Path $lib 'storport.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 interface probe link failed' }
& llvm-readobj --file-headers --coff-imports $driver
if ($LASTEXITCODE -ne 0) { throw 'PE inspection failed' }

$package = Join-Path $output 'package'
if (Test-Path $package) { Remove-Item -Recurse -Force $package }
New-Item -ItemType Directory -Force -Path $package | Out-Null
$bootPackage = $env:A04_BOOT_EXPERIMENT -eq '1'
$passivePackage = $env:A04_PASSIVE_IO -eq '1'
$inf = if ($bootPackage) { 'mt8167-msdc.inf' } elseif ($env:A04_SCRATCH_IO -eq '1') { 'mt8167-msdc-scratch.inf' } elseif ($passivePackage) { 'mt8167-msdc-readonly.inf' } else { 'a04-msdc-probe.inf' }
$sys = if ($bootPackage -or $passivePackage) { 'mt8167-msdc.sys' } else { 'a04msdcprobe.sys' }
Copy-Item $driver (Join-Path $package $sys)
Copy-Item (Join-Path $PSScriptRoot "..\..\drivers\storage\mt8167-msdc\$inf") (Join-Path $package $inf)
$inf2cat = Join-Path $kit 'bin\x86\Inf2Cat.exe'
& $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'ARM32 probe catalog generation failed' }
Write-Host "Unsigned probe package: $package"
