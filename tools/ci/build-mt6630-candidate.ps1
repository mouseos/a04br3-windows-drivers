$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$out=Join-Path $env:TEMP 'a04br3-wifi-candidate';New-Item -ItemType Directory -Force $out | Out-Null
$dir=Join-Path $root 'drivers\network\mt6630'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -O2 -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c "$dir\wdf_sdio_probe.c" -o "$out\probe.obj"
if($LASTEXITCODE) { throw 'compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib "$out\probe.obj" "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt8167-sdio-candidate.sys"
if($LASTEXITCODE) { throw 'link failed' }
Copy-Item "$dir\mt8167-sdio-candidate.inf" $out
& "$kit\bin\x86\Inf2Cat.exe" "/driver:$out" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE) { throw 'catalog failed' }
Write-Host 'Build-only disabled candidate; no radio, bus or hardware access.'
