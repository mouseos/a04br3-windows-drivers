# WDK 8.1 ARM32 diagnostic scaffold; D0 is deliberately disabled.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'mma8652-build-only'
$package = Join-Path $out 'package'
New-Item -ItemType Directory -Force $package | Out-Null
$source = Join-Path $root 'drivers\sensors\mma8652'
$objects = @()
foreach ($name in @('mma8652','mt8167_i2c','sensor_hid','wdf_mma8652')) {
    $obj = Join-Path $out "$name.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $source "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "MMA8652 ARM compilation failed: $name" }
    $objects += $obj
}
$i2c = Join-Path $out 'pio.obj'
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -c (Join-Path $root 'drivers\bus\mt8167-i2c\pio.c') -o $i2c
if ($LASTEXITCODE -ne 0) { throw 'I2C PIO compilation failed' }
$objects += $i2c
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -property installationPath
$vc = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'bin\Hostx64\arm\cl.exe') } | Select-Object -First 1
if (!$vc) { throw 'ARM32 MSVC tools not found' }
$link = Join-Path $vc.FullName 'bin\Hostx64\x64\link.exe'
& $link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$package\mma8652.sys"
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 ARM link failed' }
Copy-Item (Join-Path $source 'mma8652.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 catalog generation failed' }
$umLib = Join-Path $kit 'Lib\winv6.3\um\arm'
$statusObj = Join-Path $out 'mma8652-status.obj'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DA04_MMA8652_PROVIDER_STATUS "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\button-provider-status.c') -o $statusObj
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 status tool compilation failed' }
& $link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib $statusObj (Join-Path $umLib 'kernel32.lib') (Join-Path $umLib 'cfgmgr32.lib') "/out:$package\mma8652-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 status tool link failed' }
$readObj = Join-Path $out 'mma8652-read.obj'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\mma8652-read.c') -o $readObj
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 read tool compilation failed' }
& $link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib $readObj (Join-Path $umLib 'kernel32.lib') (Join-Path $umLib 'cfgmgr32.lib') (Join-Path $umLib 'setupapi.lib') (Join-Path $umLib 'hid.lib') "/out:$package\mma8652-read.exe"
if ($LASTEXITCODE -ne 0) { throw 'MMA8652 read tool link failed' }
Write-Host 'Experimental KMDF sensor HID package; target PnP and hardware remain unverified.'
