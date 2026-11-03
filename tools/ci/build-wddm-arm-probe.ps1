$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04-wddm-arm-probe'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c (Join-Path $root 'tools\ci\wddm-arm-probe.c') -o (Join-Path $out 'probe.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM WDDM interface compilation failed' }
$gpu = Join-Path $root 'drivers\gpu\mt8167'
$gpuObjects = @()
foreach ($name in @('firmware','firmware_runtime','firmware_start','windows_hw')) {
    $obj = Join-Path $out "$name.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$gpu\compat" "-I$gpu\fwif" -c (Join-Path $gpu "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM GE8300 compilation failed: $name" }
    $gpuObjects += $obj
}
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'probe.obj') @gpuObjects (Join-Path $lib 'displib.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') (Join-Path $lib 'BufferOverflowGDI.lib') (Join-Path $lib 'libcntpr.lib') "/out:$out\wddm-arm-probe.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM WDDM interface link failed' }
& llvm-readobj --file-headers --coff-imports (Join-Path $out 'wddm-arm-probe.sys')
if ($LASTEXITCODE -ne 0) { throw 'Unable to inspect the WDDM probe' }
Write-Host 'Non-operational ARM WDDM ABI probe; never install this file.'
