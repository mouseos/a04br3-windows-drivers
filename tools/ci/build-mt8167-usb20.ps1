param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
if (!(Test-Path (Join-Path $inc 'km\ntddk.h'))) { throw 'Windows 8.1 WDK is required' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$out = (Resolve-Path $OutputDirectory).Path
foreach ($name in @('ep0','ep0_pio','keyboard','keyboard_rx','board_host','keyboard_session','keyboard_port','controller','gpio22')) {
  $source = Join-Path $root "drivers\usb\mt8167-usb20\$name.c"
  $obj = Join-Path $out "mt8167-usb20-$name.obj"
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c $source -o $obj
  if ($LASTEXITCODE -ne 0) { throw "ARM32 WDK compilation failed: $name" }
  $headers = & llvm-readobj --file-headers $obj
  if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected COFF machine' }
}
if ($env:A04_MUSB_HOST_CORE_EXPERIMENT -eq '1') {
  $obj = Join-Path $out 'mt8167-usb20-host_core.obj'
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_HOST_CORE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c (Join-Path $root 'drivers\usb\mt8167-usb20\host_core.c') -o $obj
  if ($LASTEXITCODE -ne 0) { throw 'Experimental host core ARM32 compilation failed' }
  $headers = & llvm-readobj --file-headers $obj
  if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected host core COFF machine' }
  $control = Join-Path $out 'mt8167-usb20-host_control.obj'
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_HOST_CORE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" -include ntddk.h -c (Join-Path $root 'drivers\usb\mt8167-usb20\host_control.c') -o $control
  if ($LASTEXITCODE -ne 0) { throw 'Experimental host control ARM32 compilation failed' }
  $headers = & llvm-readobj --file-headers $control
  if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected host control COFF machine' }
  $contract = Join-Path $out 'mt8167-usb20-host_core_contract.obj'
  & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -w -fms-extensions -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MUSB_HOST_CORE_EXPERIMENT=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'tests\usb\host_core_wdk_contract.c') -o $contract
  if ($LASTEXITCODE -ne 0) { throw 'Experimental host core WDK contract failed' }
}
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$hidObj = Join-Path $out 'mt8167-usb20-wdf_keyboard.obj'
$hidControl=if($env:A04_MUSB_HID_CONTROL_EXPERIMENT -eq '1'){'1'}else{'0'}
if ($hidControl -eq '1' -and $env:A04_MUSB_HOST_CORE_EXPERIMENT -ne '1') { throw 'HID control requires host core experiment' }
$hostCore=if($env:A04_MUSB_HOST_CORE_EXPERIMENT -eq '1'){'1'}else{'0'}
& clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_MUSB_HOST_CORE_EXPERIMENT=$hostCore" "-DA04_MUSB_HID_CONTROL_EXPERIMENT=$hidControl" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $root 'drivers\usb\mt8167-usb20\wdf_keyboard.c') -o $hidObj
if ($LASTEXITCODE -ne 0) { throw 'HID transport compilation failed' }
$controllerObj = Join-Path $out 'mt8167-usb20-wdf_controller.obj'
& clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $root 'drivers\usb\mt8167-usb20\wdf_controller.c') -o $controllerObj
if ($LASTEXITCODE -ne 0) { throw 'Controller compilation failed' }
$sys = Join-Path $out 'mt8167-usb20.sys'
$hostObjects=@()
if($hidControl -eq '1'){$hostObjects=@((Join-Path $out 'mt8167-usb20-host_core.obj'),(Join-Path $out 'mt8167-usb20-host_control.obj'))}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib $hidObj $controllerObj $hostObjects (Join-Path $out 'mt8167-usb20-controller.obj') (Join-Path $out 'mt8167-usb20-board_host.obj') (Join-Path $out 'mt8167-usb20-keyboard.obj') (Join-Path $out 'mt8167-usb20-keyboard_session.obj') (Join-Path $out 'mt8167-usb20-keyboard_port.obj') (Join-Path $out 'mt8167-usb20-keyboard_rx.obj') (Join-Path $out 'mt8167-usb20-ep0.obj') (Join-Path $out 'mt8167-usb20-ep0_pio.obj') (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$sys"
if ($LASTEXITCODE -ne 0) { throw 'HID transport link failed' }
$headers = & llvm-readobj --file-headers $sys
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected HID PE machine' }
$usbInf=Join-Path $root 'drivers\usb\mt8167-usb20\mt8167-usb20.inf'
Copy-Item -LiteralPath $usbInf -Destination (Join-Path $out 'mt8167-usb20.inf') -Force
$usbDriverVer=[regex]::Match([IO.File]::ReadAllText($usbInf),'(?im)^DriverVer\s*=\s*([^\r\n]+)').Groups[1].Value.Trim()
$usbVersion=$usbDriverVer.Split(',');if($usbVersion.Count -ne 2){throw 'Invalid USB INF DriverVer'}
Write-Host 'USB20 SYS/INF built; catalog signing and hardware remain unverified.'

@{source_commit=(& git -C $root rev-parse HEAD);hardware='MediaTek MT8167 USB20';
  driver_date=$usbVersion[0];driver_version=$usbVersion[1];driver_ver=$usbDriverVer;
  kernel_codegen_flags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none');kernel_runtime_sha256=(Get-FileHash (Join-Path $kmLib 'libcntpr.lib') -Algorithm SHA256).Hash;
  host_core=$hostCore;hid_control=$hidControl;hardware_verified=$false;
  init_checkpoint_schema=1;init_checkpoint_scope='Services/A04Usb20Keyboard/Parameters';
  sys_sha256=(Get-FileHash $sys -Algorithm SHA256).Hash}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $out 'usb-manifest.json')
