# Experimental calibration-only ARM32 KMDF package; no thermal protection.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$wdfInc=Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=if ($env:A04_POWER_CANDIDATE_ROOT) { Join-Path $env:A04_POWER_CANDIDATE_ROOT 'mt8167-thermal' } else { Join-Path $tempRoot 'a04br3-thermal-diagnostic' }
New-Item -ItemType Directory -Force $out | Out-Null
$env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$dir=Join-Path $root 'drivers\power\mt8167-thermal'
$objects=@()
$shared = if ($env:A04_THERMAL_SHARED_PARENT -eq '1') { '1' } else { '0' }
$lease = if ($env:A04_THERMAL_LEASE_DIAG -eq '1') { '1' } else { '0' }
$oneshot = if ($env:A04_THERMAL_ONESHOT_TEST -eq '1') { '1' } else { '0' }
$monitor = if ($env:A04_THERMAL_MONITOR_TEST -eq '1') { '1' } else { '0' }
$codecAdc = if ($env:A04_THERMAL_CODEC_ADC_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($codecAdc -eq '1' -and ($shared -ne '1' -or $oneshot -ne '1')) { throw 'codec ADC requires shared one-shot' }
if ($oneshot -eq '1' -and $monitor -ne '1') { throw 'one-shot requires monitor infrastructure' }
if ($monitor -eq '1' -and $lease -ne '1') { throw 'monitor requires lease diagnostic' }
$native = if ($env:A04_THERMAL_NATIVE_PARENT -eq '1') { '1' } else { '0' }
$names = @('wdf_thermal','thermal','boot_record')
if ($native -eq '1') { $names=@('wdf_native_thermal','thermal','boot_record','oneshot','completion','freshness','hardware','readout') }
elseif ($monitor -eq '1') { $names += @('monitor','completion','freshness','hardware','readout') }
if ($native -ne '1' -and $oneshot -eq '1') { $names += 'oneshot' }
foreach($name in $names) {
 $obj=Join-Path $out "$name.obj"
 & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -D__ldrexd=__a04_power_wdk81_ldrexd_declaration -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_THERMAL_SHARED_PARENT=$shared" "-DA04_THERMAL_ONESHOT_TEST=$oneshot" "-DA04_THERMAL_LEASE_DIAG=$lease" "-DA04_THERMAL_MONITOR_TEST=$monitor" "-DA04_THERMAL_CODEC_ADC_DIAGNOSTIC=$codecAdc" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$dir" -c (Join-Path $dir "$name.c") -o $obj
 if($LASTEXITCODE -ne 0){throw "thermal compile failed: $name"}
 $objects += $obj
}
if ($codecAdc -eq '1') {
 $obj=Join-Path $out 'codec_adc_probe.obj'
 & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root 'drivers\bus\mt8167-shared\codec_adc_probe.c') -o $obj
 if($LASTEXITCODE -ne 0){throw 'codec ADC compile failed'}
 $objects += $obj
 $obj=Join-Path $out 'codec_adc_session.obj'
 & clang --target=thumbv7-pc-windows-msvc -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root 'drivers\bus\mt8167-shared\codec_adc_session.c') -o $obj
 if($LASTEXITCODE -ne 0){throw 'codec ADC session compile failed'}
 $objects += $obj
}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $km 'ntoskrnl.lib') (Join-Path $km 'hal.lib') (Join-Path $km 'bufferoverflowfastfailk.lib') (Join-Path $km 'libcntpr.lib') "/out:$out\mt8167-thermal.sys"
if($LASTEXITCODE -ne 0){throw 'thermal link failed'}
Copy-Item (Join-Path $dir $(if($native -eq '1'){'mt8167-thermal-native.inf'}else{'mt8167-thermal.inf'})) (Join-Path $out 'mt8167-thermal.inf') -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM
if($LASTEXITCODE -ne 0){throw 'thermal catalog failed'}

@($objects | ForEach-Object {@{path=$_;sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}}) | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $out "kernel-objects.json")
