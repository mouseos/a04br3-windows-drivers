$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$include = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$source = Join-Path $PSScriptRoot '..\..\drivers\storage\mt8167-msdc\miniport.c'
$output = Join-Path $env:RUNNER_TEMP 'a04br3-wdk81-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$object = Join-Path $output 'miniport.obj'
$pioObject = Join-Path $output 'pio.obj'
$pioSource = Join-Path $PSScriptRoot '..\..\drivers\storage\mt8167-msdc\pio.c'
$driver = Join-Path $output 'miniport.sys'
$extraObjects = @()
$probeDefines = @()
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
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib $object $pioObject @extraObjects (Join-Path $lib 'storport.lib') (Join-Path $lib 'ntoskrnl.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 interface probe link failed' }
& llvm-readobj --file-headers --coff-imports $driver
if ($LASTEXITCODE -ne 0) { throw 'PE inspection failed' }

$package = Join-Path $output 'package'
New-Item -ItemType Directory -Force -Path $package | Out-Null
Copy-Item $driver (Join-Path $package 'a04msdcprobe.sys')
$inf = if ($env:A04_BOOT_EXPERIMENT -eq '1') { 'a04-msdc-boot-experiment.inf' } else { 'a04-msdc-probe.inf' }
Copy-Item (Join-Path $PSScriptRoot "..\..\drivers\storage\mt8167-msdc\$inf") (Join-Path $package 'a04-msdc-probe.inf')
$inf2cat = Join-Path $kit 'bin\x86\Inf2Cat.exe'
& $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'ARM32 probe catalog generation failed' }
Write-Host "Unsigned probe package: $package"
