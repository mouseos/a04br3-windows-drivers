$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04-button-provider'
New-Item -ItemType Directory -Force $out | Out-Null
$dir = Join-Path $root 'drivers\input\a04br3-buttons'
$objects = @()
$assumePristine = if ($env:A04_ASSUME_PRISTINE_BUTTON_QUEUE -eq '1') { '1' } else { '0' }
$buttonTrace = if ($env:A04_BUTTON_TRACE -eq '1') { '1' } else { '0' }
foreach ($name in @('wdf_provider','button_hardware','pmic_read','button_device','button_pipeline','button_state')) {
    $obj = Join-Path $out "$name.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_ASSUME_PRISTINE_BUTTON_QUEUE=$assumePristine" "-DA04_BUTTON_TRACE=$buttonTrace" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$dir" -c (Join-Path $dir "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM32 button provider compilation failed: $name" }
    $objects += $obj
}
$driver = Join-Path $out 'a04buttonprovider.sys'
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 button provider link failed' }
$headers = & llvm-readobj --file-headers $driver
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected provider PE format' }
Copy-Item (Join-Path $dir 'a04-button-provider.inf') (Join-Path $out 'a04-button-provider.inf')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Button provider catalog generation failed' }
Write-Host "Experimental PnP provider built; assume pristine queue: $assumePristine; UART trace: $buttonTrace. Wake is not implemented."
