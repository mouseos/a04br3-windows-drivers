$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$out=Join-Path $env:RUNNER_TEMP 'usb-bqhr';New-Item -ItemType Directory -Force $out | Out-Null
$files=@('drivers\usb\mt8167-usb20\bqhr_observe\bqhr_driver.c','drivers\power\bq24160\bqhr_acpi.c','drivers\power\bq24160\bqhr_diagnostic.c');$objects=@()
foreach($i in 0..2){$obj=Join-Path $out "$i.obj";& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c (Join-Path $root $files[$i]) -o $obj;if($LASTEXITCODE){throw 'BQHR compile failed'};$objects+=$obj}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdf 'wdfdriverentry.lib') (Join-Path $wdf 'wdfldr.lib') (Join-Path $km 'ntoskrnl.lib') (Join-Path $km 'hal.lib') (Join-Path $km 'bufferoverflowfastfailk.lib') "/out:$out\bqhr-observe.sys"
if($LASTEXITCODE){throw 'BQHR link failed'}
Copy-Item (Join-Path $root 'drivers\usb\mt8167-usb20\bqhr_observe\bqhr-observe.inf') $out
