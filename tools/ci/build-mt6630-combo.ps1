# Private driver-only candidate. No parent/audio/media build and no deployment.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$NdisOverlay)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New source-local output required'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
if((Get-FileHash $NdisOverlay -Algorithm SHA256).Hash -ne 'A4D318943678AE45F3D29A51685AB81A078DF814377561DA68B3150D5FBA0E7F'){throw 'Pinned WDK8.1 overlay required'}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\overlay","$out\tmp","$out\provider","$out\bluetooth"|Out-Null
Copy-Item $NdisOverlay "$out\overlay\ndis.h"
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$wdf="$kit\Lib\wdf\kmdf\arm\1.13"
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-w','-ffunction-sections','-fdata-sections','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
 $ndisFlags=@('-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1',"-I$out\overlay")+$common
 $records=@();$objects=@()
 # wmt_bridge's optional callback changes nested layout; rebuild all dependent
 # driver TUs. No unsafe reuse of1013 objects with the old nested structure.
 foreach($name in @('windows_irq_firmware','windows_session_irq','windows_irq_work','windows_irq','windows_session','windows_diagnostic','windows_calibration','windows_nvram_reader','windows_firmware','sha256','windows_board','resource_lease','bringup','board_sequence','msdc2_pio','power_bridge','power_plan','transport','direct','calibration','nvram_sector','wlan_firmware','windows_msdc2','lifetime','identity','enumerate','control_start','regulatory','wmt_bridge')){
  $source="drivers/network/mt6630/$name.c";$obj="$out\core\$name.obj"
  & clang @ndisFlags -c "$root\$source" -o $obj;if($LASTEXITCODE){throw "Core compile failed $name"}
  $objects+=$obj;$records+=@{source=$source;source_sha256=(Get-FileHash "$root\$source" -Algorithm SHA256).Hash;object="core/$name.obj";sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 foreach($name in @('checkpoint','ndis_runtime','native_control','native_contract','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx','combo_broker')){
  $source="drivers/network/mt6630/$name.c";$obj="$out\ndis\$name.obj"
  & clang @ndisFlags -c "$root\$source" -o $obj;if($LASTEXITCODE){throw "NDIS compile failed $name"}
  $objects+=$obj;$records+=@{source=$source;source_sha256=(Get-FileHash "$root\$source" -Algorithm SHA256).Hash;object="ndis/$name.obj";sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 $source='drivers/bluetooth/mt6630/transport.c';$obj="$out\bt-transport.obj"
 & clang @ndisFlags -c "$root\$source" -o $obj;if($LASTEXITCODE){throw 'BT transport compile failed'}
 $objects+=$obj;$records+=@{source=$source;source_sha256=(Get-FileHash "$root\$source" -Algorithm SHA256).Hash;object='bt-transport.obj';sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys"
 if($LASTEXITCODE){throw 'Combo provider link failed'}
 $source='drivers/bluetooth/mt6630/combo_client.c';$obj="$out\combo-client.obj"
 $clientFlags=$common+@('-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',"-I$inc\wdf\kmdf\1.13")
 & clang @clientFlags -c "$root\$source" -o $obj;if($LASTEXITCODE){throw 'Combo Bluetooth client compile failed'}
 $records+=@{source=$source;source_sha256=(Get-FileHash "$root\$source" -Algorithm SHA256).Hash;object='combo-client.obj';sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref $obj "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\bluetooth\mt6630-combo-bluetooth.sys"
 if($LASTEXITCODE){throw 'Combo Bluetooth client link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\bluetooth"
 $clang=(Get-Command clang).Source
 @{source_commit=$commit;source_dirty=$false;compiler_sha256=(Get-FileHash $clang -Algorithm SHA256).Hash;ndis_overlay_sha256=(Get-FileHash "$out\overlay\ndis.h" -Algorithm SHA256).Hash;objects=$records;ndis_flags=$ndisFlags;client_flags=$clientFlags;parent_built=$false;firmware_embedded=$false;device_calibration_embedded=$false;hardware_verified=$false;standard_stack_compatible=$false;combo_feature_default=0;combo_feature_build=1;runtime_checkpoint_build=1014;bt_driver_version=2008;reused_objects=0;recompile_reason='nested WMT bridge layout changed; rebuild driver dependents only'}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'COMBO_DRIVER_ONLY_BUILD_COMPLETE'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
