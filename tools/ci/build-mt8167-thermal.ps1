# Experimental calibration-only ARM32 KMDF package; no thermal protection.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$wdfInc=Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=Join-Path $tempRoot 'a04br3-thermal-diagnostic'
New-Item -ItemType Directory -Force $out | Out-Null
$dir=Join-Path $root 'drivers\power\mt8167-thermal'
$objects=@()
$lease = if ($env:A04_THERMAL_LEASE_DIAG -eq '1') { '1' } else { '0' }
$oneshot = if ($env:A04_THERMAL_ONESHOT_TEST -eq '1') { '1' } else { '0' }
$monitor = if ($env:A04_THERMAL_MONITOR_TEST -eq '1') { '1' } else { '0' }
if ($oneshot -eq '1' -and $monitor -ne '1') { throw 'one-shot requires monitor infrastructure' }
if ($monitor -eq '1' -and $lease -ne '1') { throw 'monitor requires lease diagnostic' }
$names = @('wdf_thermal','thermal')
if ($monitor -eq '1') { $names += @('monitor','completion','freshness','hardware','readout') }
if ($oneshot -eq '1') { $names += 'oneshot' }
foreach($name in $names) {
 $obj=Join-Path $out "$name.obj"
 & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_THERMAL_ONESHOT_TEST=$oneshot" "-DA04_THERMAL_LEASE_DIAG=$lease" "-DA04_THERMAL_MONITOR_TEST=$monitor" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$dir" -c (Join-Path $dir "$name.c") -o $obj
 if($LASTEXITCODE -ne 0){throw "thermal compile failed: $name"}
 $objects += $obj
}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $km 'ntoskrnl.lib') (Join-Path $km 'hal.lib') (Join-Path $km 'bufferoverflowfastfailk.lib') (Join-Path $km 'libcntpr.lib') "/out:$out\mt8167-thermal.sys"
if($LASTEXITCODE -ne 0){throw 'thermal link failed'}
Copy-Item (Join-Path $dir 'mt8167-thermal.inf') $out -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE -ne 0){throw 'thermal catalog failed'}
