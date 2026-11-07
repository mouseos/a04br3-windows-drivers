# Explicit build, not a runner launcher. Windows WDK8.1 ARM + clang/lld required.
param([string]$OutputDirectory,[string]$SharedParentDirectory)
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-runtime'}
$taskOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$taskOut.StartsWith($root.Path+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
if($SharedParentDirectory){
 $SharedParentDirectory=[IO.Path]::GetFullPath($SharedParentDirectory)
 if(!$SharedParentDirectory.StartsWith($root.Path+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'SharedParentDirectory must be inside this checkout'}
 $parentBinary=Join-Path $SharedParentDirectory 'mt8167-shared.sys'
 $parentManifest=Get-Content -Raw (Join-Path $SharedParentDirectory 'parent-manifest.json')|ConvertFrom-Json
 foreach($flag in @('A04_SHARED_WIFI_RUNTIME','A04_MT6392_RTC32K_DIAGNOSTIC','A04_SHARED_WIFI_IRQ_DIAGNOSTIC')){
  if($parentManifest.flags.$flag -ne '1'){throw "Shared parent lacks required Wi-Fi capability: $flag"}
 }
 if($parentManifest.sys_sha256 -ne (Get-FileHash $parentBinary -Algorithm SHA256).Hash){throw 'Shared parent hash does not match its capabilities manifest'}
 if($parentManifest.source_dirty -ne $false -or $parentManifest.source_commit -notmatch '^[0-9a-f]{40}$'){throw 'Shared parent needs a clean fixed source commit'}
 # A Wi-Fi-only fix does not require rebuilding the audio/USB parent. Require
 # its actual capabilities, binary hash and identical public interface blob.
 $interfacePath='drivers/bus/mt8167-shared/wifi_interface.h'
 $parentInterface=(& git -C $root rev-parse "$($parentManifest.source_commit):$interfacePath")
 if($LASTEXITCODE){throw 'Parent interface source is unavailable'}
 $currentInterface=(& git -C $root rev-parse "HEAD:$interfacePath")
 if($LASTEXITCODE -or $parentInterface -ne $currentInterface){throw 'Shared parent Wi-Fi interface differs'}
}
if(Test-Path -LiteralPath $taskOut){throw 'Use a new runtime output directory for fixed-source package provenance'}
$fixedCommit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or $fixedCommit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed source commit required'}
$dirty=(& git -C $root status --porcelain --untracked-files=normal);if($LASTEXITCODE -or $dirty){throw 'Use a clean committed source checkout'}
New-Item -ItemType Directory -Force $taskOut | Out-Null
# Keep every intermediate inside this checkout. No external TEMP staging.
$priorTmp=$env:TMP;$priorTemp=$env:TEMP;$priorRunner=$env:RUNNER_TEMP
$names=@('A04_WIFI_SESSION_DIAGNOSTIC','A04_WIFI_IRQ_DIAGNOSTIC','A04_WIFI_CALIBRATION_DIAGNOSTIC','A04_SHARED_WIFI_RUNTIME','A04_MT6392_RTC32K_DIAGNOSTIC','A04_SHARED_WIFI_IRQ_DIAGNOSTIC','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_TOUCH_D0_EXPERIMENT','A04_SHARED_APMIXED_RESOURCE')
$prior=@{};foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name);[Environment]::SetEnvironmentVariable($name,'1')}
try {
 $env:TMP=$taskOut;$env:TEMP=$taskOut;$env:RUNNER_TEMP=$taskOut
 & "$PSScriptRoot\build-mt6630-core.ps1"
 if(!$SharedParentDirectory){
  & "$PSScriptRoot\build-msdc-parent-probe.ps1"
  $parentBinary=Join-Path $taskOut 'a04-msdc-parent-probe\mt8167-shared.sys'
 }
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
 foreach($name in @('checkpoint','ndis_runtime','native_control','native_contract','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx')) {
  $obj=Join-Path $taskOut "$name.obj"
  & clang --target=thumbv7-pc-windows-msvc -w -std=c11 -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -D_WINSOCK_DEPRECATED_NO_WARNINGS=1 -DNDIS_MINIPORT_DRIVER -DNDIS630_MINIPORT -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_WIFI_SESSION_DIAGNOSTIC=1 -DA04_WIFI_IRQ_DIAGNOSTIC=1 -DA04_WIFI_CALIBRATION_DIAGNOSTIC=1 "-I$overlay" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\$name.c" -o $obj
  if($LASTEXITCODE){throw "$name compile failed"};$objects+=$obj
 }
 # All Wi-Fi/core objects use scalar ARM kernel codegen above. Integer helper
 # exception paths and GS support come from the WDK kernel runtime libraries;
 # never provide local cookie/FP/helper stubs or use user-mode CRT libraries.
 # Avoid duplicate native_tx from the core set; never wildcard another driver.
 $core=Join-Path $taskOut 'a04br3-mt6630-core'
 $objects+=Get-ChildItem "$core\*.obj" | Where-Object { $_.BaseName -ne 'native_tx' } | ForEach-Object {$_.FullName}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$taskOut\mt6630-runtime.map" "/out:$taskOut\mt6630-runtime.sys"
 if($LASTEXITCODE){throw 'Native runtime link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" $taskOut
 & "$PSScriptRoot\build-mt6630-wlan-probe.ps1" -OutputDirectory (Join-Path $taskOut 'wlan-probe')
 [ordered]@{driver_date='11/06/2026';driver_version='2026.11.6.1006';driver_ver='11/06/2026,2026.11.6.1006';source_commit=$fixedCommit;source_dirty=$false;wifi_source_tree=(& git -C $root rev-parse HEAD:drivers/network/mt6630);critical_flags=@{A04_WIFI_SESSION_DIAGNOSTIC=1;A04_WIFI_IRQ_DIAGNOSTIC=1;A04_WIFI_CALIBRATION_DIAGNOSTIC=1};build_recipe_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash;core_recipe_sha256=(Get-FileHash (Join-Path $PSScriptRoot 'build-mt6630-core.ps1') -Algorithm SHA256).Hash;kernel_codegen_flags=@('-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector');kernel_runtime_libraries=@('ndis.lib','ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib','libcntpr.lib');kernel_runtime_hashes=@(foreach($lib in @('ndis.lib','ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib','libcntpr.lib')){[ordered]@{name=$lib;sha256=(Get-FileHash (Join-Path $km $lib) -Algorithm SHA256).Hash}});inf_sha256=(Get-FileHash "$taskOut\mt6630-runtime.inf" -Algorithm SHA256).Hash;deployment_ready=$false;checkpoint_schema_version=1;checkpoint_driver_build=1006;diagnostic_substage_results=$true;checkpoint_capacity=64;checkpoint_registry='Services/Mt6630Runtime/Parameters/NativeCheckpoint';checkpoint_tail_flush_proven=$false;native_tx_queue_depth=64;native_rx_pool_depth=64;scan_ssid_capacity=4;excluded_mac_capacity=4;implemented_mtu=2304;native_data_tc=1;native_be_initial_pages=468;prepared_buffer_mtu=2304;unreachable_default_ms=2000;unreachable_criterion='actual gen3 beacon-loss evidence plus elapsed time since verified peer RX';accepted_power_levels=@(0,8,16);proprietary_power_level24=$false;unassociated_rf_idle='CID17 inactive, real TX/IST drain, FW ownership acknowledgement';idle_poll_hif_access=$false;wake_before_scan_connect=$true;rf_power_measured=$false;deployment_blockers=@('Native1006 diagnostic substage tracing requires WDK link and target validation; run42 failure inside SESSION_DIAGNOSTIC remains unresolved');shared_parent_path=$parentBinary;shared_parent_source_commit=$(if($SharedParentDirectory){$parentManifest.source_commit}else{git -C $root rev-parse HEAD});shared_parent_interface_blob=$(if($SharedParentDirectory){$parentInterface}else{git -C $root rev-parse HEAD:drivers/bus/mt8167-shared/wifi_interface.h});shared_parent_sha256=(Get-FileHash $parentBinary -Algorithm SHA256).Hash;shared_parent_reused=[bool]$SharedParentDirectory;probe_manifest_path='wlan-probe/probe-manifest.json';probe_manifest_sha256=(Get-FileHash (Join-Path $taskOut 'wlan-probe\probe-manifest.json') -Algorithm SHA256).Hash;native_runtime=1;radio_mode='2.4GHz ERP20 OPEN/NONE or RSNA-PSK/CCMP; OS supplicant';firmware_embedded=0;device_calibration_embedded=0;hardware_verified=0;wdk_link_succeeded=1;sys_sha256=(Get-FileHash "$taskOut\mt6630-runtime.sys" -Algorithm SHA256).Hash} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 "$taskOut\manifest.json"
 Write-Output "Unsigned Native miniport and shared parent: $taskOut. Requires target signing and PnP validation by main operator."
} finally {
 $env:TMP=$priorTmp;$env:TEMP=$priorTemp;$env:RUNNER_TEMP=$priorRunner
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name])}
}
