# Explicit build, not a runner launcher. Windows WDK8.1 ARM + clang/lld required.
param([string]$OutputDirectory,[Alias('SkipParent')][switch]$SkipSharedParent)
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-runtime'}
$taskOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$taskOut.StartsWith($root.Path+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
 throw 'OutputDirectory must be inside this checkout'
}
New-Item -ItemType Directory -Force $taskOut | Out-Null
# Keep every intermediate inside this checkout. No external TEMP staging.
$priorTmp=$env:TMP;$priorTemp=$env:TEMP;$priorRunner=$env:RUNNER_TEMP
$names=@('A04_WIFI_SESSION_DIAGNOSTIC','A04_WIFI_IRQ_DIAGNOSTIC','A04_WIFI_CALIBRATION_DIAGNOSTIC','A04_SHARED_WIFI_RUNTIME','A04_MT6392_RTC32K_DIAGNOSTIC','A04_SHARED_WIFI_IRQ_DIAGNOSTIC','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_TOUCH_D0_EXPERIMENT','A04_SHARED_APMIXED_RESOURCE')
$prior=@{};foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name);[Environment]::SetEnvironmentVariable($name,'1')}
try {
 $env:TMP=$taskOut;$env:TEMP=$taskOut;$env:RUNNER_TEMP=$taskOut
 & "$PSScriptRoot\build-mt6630-core.ps1"
 if(!$SkipSharedParent){& "$PSScriptRoot\build-msdc-parent-probe.ps1"}
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
 # Reuse the checked NDIS token-paste overlay and compile Native I/O bridges.
 $overlay=Join-Path $taskOut 'a04br3-ndis-candidate'
 New-Item -ItemType Directory -Force $overlay | Out-Null
 $header=[IO.File]::ReadAllText((Join-Path $inc 'km\ndis.h'))
 $old='#define NDIS_API_VERSION_AVAILABLE(major,minor) ((((0x ## major) << 8) + (0x ## minor)) >= NDIS_MIN_API)'
 $new="#define A04_NDIS_HEX_I(x) 0x ## x`r`n#define A04_NDIS_HEX(x) A04_NDIS_HEX_I(x)`r`n#define NDIS_API_VERSION_AVAILABLE(major,minor) (((A04_NDIS_HEX(major) << 8) + A04_NDIS_HEX(minor)) >= NDIS_MIN_API)"
 if(!$header.Contains($old)){throw 'Unrecognized WDK8.1 NDIS header'}
 [IO.File]::WriteAllText((Join-Path $overlay 'ndis.h'),$header.Replace($old,$new),[Text.Encoding]::ASCII)
 $objects=@()
 foreach($name in @('ndis_runtime','native_control','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx')) {
  $obj=Join-Path $taskOut "$name.obj"
  & clang --target=thumbv7-pc-windows-msvc -w -std=c11 -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -D_WINSOCK_DEPRECATED_NO_WARNINGS=1 -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_WIFI_SESSION_DIAGNOSTIC=1 -DA04_WIFI_IRQ_DIAGNOSTIC=1 -DA04_WIFI_CALIBRATION_DIAGNOSTIC=1 "-I$overlay" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\$name.c" -o $obj
  if($LASTEXITCODE){throw "$name compile failed"};$objects+=$obj
 }
 # Avoid duplicate native_tx from the core set; never wildcard another driver.
 $core=Join-Path $taskOut 'a04br3-mt6630-core'
 $objects+=Get-ChildItem "$core\*.obj" | Where-Object { $_.BaseName -ne 'native_tx' } | ForEach-Object {$_.FullName}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$taskOut\mt6630-runtime.sys"
 if($LASTEXITCODE){throw 'Native runtime link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" $taskOut
 & "$PSScriptRoot\build-mt6630-wlan-probe.ps1" -OutputDirectory (Join-Path $taskOut 'wlan-probe')
 [ordered]@{source_commit=(git -C $root rev-parse HEAD);shared_parent_built=(!$SkipSharedParent);probe_manifest_path='wlan-probe/probe-manifest.json';probe_manifest_sha256=(Get-FileHash (Join-Path $taskOut 'wlan-probe\probe-manifest.json') -Algorithm SHA256).Hash;native_runtime=1;radio_mode='2.4GHz ERP20 OPEN/NONE or RSNA-PSK/CCMP; OS supplicant';firmware_embedded=0;device_calibration_embedded=0;hardware_verified=0;wdk_link_succeeded=1;sys_sha256=(Get-FileHash "$taskOut\mt6630-runtime.sys" -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content -Encoding UTF8 "$taskOut\manifest.json"
 Write-Output "Unsigned Native miniport (shared parent skipped=$SkipSharedParent): $taskOut. Requires target signing and PnP validation by main operator."
} finally {
 $env:TMP=$priorTmp;$env:TEMP=$priorTemp;$env:RUNNER_TEMP=$priorRunner
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name])}
}
