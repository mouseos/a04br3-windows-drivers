param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
if (!(Test-Path (Join-Path $inc 'km\ntddk.h'))) { throw 'Windows 8.1 WDK is required' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
foreach ($name in @('ep0','ep0_pio','keyboard','keyboard_rx','board_host')) {
  $source = Join-Path $root "drivers\usb\mt8167-usb20\$name.c"
  $obj = Join-Path $out "mt8167-usb20-$name.obj"
  & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c $source -o $obj
  if ($LASTEXITCODE -ne 0) { throw "ARM32 WDK compilation failed: $name" }
  $headers = & llvm-readobj --file-headers $obj
  if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected COFF machine' }
}
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$hidObj = Join-Path $out 'mt8167-usb20-wdf_keyboard.obj'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $root 'drivers\usb\mt8167-usb20\wdf_keyboard.c') -o $hidObj
if ($LASTEXITCODE -ne 0) { throw 'HID transport compilation failed' }
$sys = Join-Path $out 'mt8167-usb20-hid-transport.sys'
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib $hidObj (Join-Path $out 'mt8167-usb20-keyboard.obj') (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') "/out:$sys"
if ($LASTEXITCODE -ne 0) { throw 'HID transport link failed' }
$headers = & llvm-readobj --file-headers $sys
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected HID PE machine' }
Write-Host 'HID transport linked; no controller connection or installable package.'
