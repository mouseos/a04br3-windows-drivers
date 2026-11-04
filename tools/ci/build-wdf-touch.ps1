$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04-wdf-touch'
New-Item -ItemType Directory -Force $out | Out-Null
$dir = Join-Path $root 'drivers\input\bu21025'
$objects = @()
$trace = if ($env:A04_TOUCH_TRACE -eq '1') { '1' } else { '0' }
$sharedParent = if ($env:A04_TOUCH_SHARED_PARENT -eq '1') { '1' } else { '0' }
foreach ($source in @('wdf_touch','board_init','bu21025','mt8167_adapter','device','pipeline','contact','coordinates','touch_hid')) {
    $obj = Join-Path $out "$source.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_TOUCH_TRACE=$trace" "-DA04_TOUCH_SHARED_PARENT=$sharedParent" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$dir" -c (Join-Path $dir "$source.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM32 touch compilation failed: $source" }
    $objects += $obj
}
$i2cObj = Join-Path $out 'i2c.obj'
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root 'drivers\bus\mt8167-i2c\pio.c') -o $i2cObj
if ($LASTEXITCODE -ne 0) { throw 'ARM32 I2C compilation failed' }
$objects += $i2cObj
$driver = Join-Path $out 'bu21025.sys'
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 touch link failed' }
& llvm-readobj --file-headers $driver
if ($LASTEXITCODE -ne 0) { throw 'Unable to inspect output' }
if ($sharedParent -eq '1') { Write-Host 'Shared-parent compile-only build; do not install.'; return }
Copy-Item (Join-Path $dir 'bu21025.inf') (Join-Path $out 'bu21025.inf')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Touch catalog generation failed' }
Write-Host 'Experimental touch SYS built; PnP, HID, power and physical dimensions are unverified.'
