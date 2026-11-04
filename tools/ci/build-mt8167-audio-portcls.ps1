# Build-only PortCls adapter. StartDevice always fails; no install package.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$out = Join-Path $env:TEMP 'a04-mt8167-portcls'
New-Item -ItemType Directory -Force $out | Out-Null
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\adapter.cpp') -o (Join-Path $out 'adapter.obj')
if ($LASTEXITCODE -ne 0) { throw 'PortCls ARM32 compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'adapter.obj') (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') "/out:$out\mt8167-audio-disabled.sys"
if ($LASTEXITCODE -ne 0) { throw 'PortCls ARM32 link failed' }
foreach ($name in @('dl1','dl1_engine','ring','irq','broker_policy')) {
    & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root "drivers\audio\mt8167-afe\$name.c") -o (Join-Path $out "$name.obj")
    if ($LASTEXITCODE -ne 0) { throw "ARM32 helper compilation failed: $name" }
}
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\wavert_dma.cpp') -o (Join-Path $out 'wavert_dma.obj')
if ($LASTEXITCODE -ne 0) { throw 'WaveRT buffer helper compilation failed' }
Write-Host 'Built disabled PortCls skeleton and unconnected helper objects; StartDevice always fails.'
