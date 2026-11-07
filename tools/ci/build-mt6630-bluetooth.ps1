# WDK8.1 ARM32: exclusive Bluetooth radio profile; no signing/deployment/runner.
param([string]$OutputDirectory,[switch]$BuildSharedParent,[string]$ReuseParentDirectory,[string]$ReuseDirectory,[string]$ReuseManifestSha256)
$ErrorActionPreference='Stop'
if($BuildSharedParent -and $ReuseParentDirectory){throw 'Choose BuildSharedParent or ReuseParentDirectory'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-bluetooth'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE){throw 'Cannot identify source commit'}
$dirty=@(& git -C $root status --porcelain)
if($LASTEXITCODE -or $dirty.Count){throw 'Build requires a clean fixed source commit'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$names=@('TEMP','TMP','RUNNER_TEMP','A04_WIFI_IRQ_DIAGNOSTIC','A04_WIFI_SESSION_DIAGNOSTIC','A04_WIFI_CALIBRATION_DIAGNOSTIC')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$manifest=Join-Path $out 'manifest.json'
if(Test-Path $manifest){Move-Item $manifest (Join-Path $out ('manifest.previous.'+[Guid]::NewGuid().ToString('N')+'.json'))}
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$out
 $env:A04_WIFI_IRQ_DIAGNOSTIC='1';$env:A04_WIFI_SESSION_DIAGNOSTIC='0';$env:A04_WIFI_CALIBRATION_DIAGNOSTIC='0'
 $parentSource=$null;$parentHash=$null;$parentManifestHash=$null;$parentInf=$null
 if($BuildSharedParent -or $ReuseParentDirectory){
  # Reuse the full Audio v11 + USB + input parent, changing just its single
  # MSDC2 child profile. Never create a second physical host PDO.
  & "$PSScriptRoot\build-mt8167-first-playback.ps1" -IntegratedUsb -BluetoothProfile -ParentOnly -ReuseParentDirectory $ReuseParentDirectory -OutputDirectory (Join-Path $out 'audio-usb')
  $parentManifest=Get-Content (Join-Path $out 'audio-usb\manifest.json') -Raw|ConvertFrom-Json
  if(!$parentManifest.parent_only -or $parentManifest.client_built -or $parentManifest.audio_built -or $parentManifest.usb_built -or
     $parentManifest.diagnostic_interface -ne 11 -or !$parentManifest.integrated_usb -or
     $parentManifest.parent_flags.A04_SHARED_BT_PROFILE -ne '1' -or
     $parentManifest.parent_flags.A04_SHARED_WIFI_RUNTIME -ne '1'){
    throw 'Bluetooth retained parent profile mismatch'
  }
  $parentInf=@{source_commit=$parentManifest.inf_source_commit;source_path=$parentManifest.inf_source_path;driver_ver=$parentManifest.inf_driver_ver;sha256=$parentManifest.inf_sha256}
  $parentSource=$parentManifest.parent_source_commit;$parentHash=$parentManifest.sys_sha256
  $parentManifestHash=(Get-FileHash (Join-Path $out 'audio-usb\a04-msdc-parent-probe\manifest.json') -Algorithm SHA256).Hash

 }
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $km=Join-Path $kit 'Lib\winv6.3\km\arm';$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
 $kernelCodegenFlags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-w','-ffunction-sections','-fdata-sections','-fms-extensions','-ffreestanding','-O2','-fno-stack-protector',
  '-D__ldrexd=__bt_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',
  '-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_SESSION_DIAGNOSTIC=0','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=0',
  "-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13")+$kernelCodegenFlags
 # Source/flags/dependency/output-hash checked object cache. The first clean
 # build creates it; later BT-only changes need not rebuild shared WiFi/WMT TUs.
 $cache=$null;$records=@();$compiled=@();$reused=@()
 $clangPath=(Get-Command clang -ErrorAction Stop).Source;$clangHash=(Get-FileHash $clangPath -Algorithm SHA256).Hash
 $resource=(& clang -print-resource-dir);if($LASTEXITCODE){throw 'Clang resource directory missing'}
 $sdkFiles=@(Get-ChildItem $inc,(Join-Path $resource 'include') -File -Recurse|Where-Object {$_.Extension -in @('.h','.hpp','.inl')}|Sort-Object FullName)
 $sdkText=($sdkFiles|ForEach-Object {$_.FullName+' '+(Get-FileHash $_.FullName -Algorithm SHA256).Hash}) -join "`n"
 $hasher=[Security.Cryptography.SHA256]::Create()
 try {$sdkHash=([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($sdkText)))).Replace('-','')} finally {$hasher.Dispose()}
 if($ReuseDirectory){
  $ReuseDirectory=[IO.Path]::GetFullPath($ReuseDirectory)
  $cachePath=Join-Path $ReuseDirectory 'manifest.json'
  if($ReuseManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash $cachePath -Algorithm SHA256).Hash -ne $ReuseManifestSha256){throw 'Fixed reuse manifest hash required'}
  $cache=Get-Content -Raw $cachePath|ConvertFrom-Json
  if($cache.source_dirty -or $cache.object_cache_version -ne 1 -or $cache.clang_sha256 -ne $clangHash -or $cache.sdk_headers_sha256 -ne $sdkHash -or
     ($cache.kernel_compile_flags|ConvertTo-Json -Compress) -ne ($common|ConvertTo-Json -Compress) -or
     $cache.crt_library_sha256 -ne (Get-FileHash "$km\libcntpr.lib" -Algorithm SHA256).Hash){throw 'Incompatible fixed object cache'}
 }
 $networkHeaders=@(Get-ChildItem "$root\drivers\network\mt6630" -File -Recurse -Filter '*.h'|ForEach-Object {$_.FullName.Substring($root.Length+1)})+@('drivers/bus/mt8167-shared/wifi_interface.h')
 function Build-Unit([string]$relative,[string]$object,[string[]]$dependencies){
  $source=Join-Path $root $relative;$sourceHash=(Get-FileHash $source -Algorithm SHA256).Hash
  $deps=@($dependencies|Sort-Object -Unique|ForEach-Object {@{path=$_;sha256=(Get-FileHash (Join-Path $root $_) -Algorithm SHA256).Hash}})
  $hit=$false
  if($cache){
   $prior=@($cache.object_records|Where-Object {$_.source -eq $relative -and $_.object -eq [IO.Path]::GetFileName($object)})
   if($prior.Count -eq 1 -and $prior[0].source_sha256 -eq $sourceHash -and $prior[0].dependencies.Count -eq $deps.Count){
    $hit=$true
    foreach($dep in $deps){$old=@($prior[0].dependencies|Where-Object {$_.path -eq $dep.path -and $_.sha256 -eq $dep.sha256});if($old.Count -ne 1){$hit=$false}}
    if($hit){$oldObj=Join-Path $ReuseDirectory $prior[0].object
     if(!(Test-Path $oldObj) -or (Get-FileHash $oldObj -Algorithm SHA256).Hash -ne $prior[0].sha256){throw 'Cached object hash mismatch'}
     Copy-Item $oldObj $object;$script:reused+=$relative
    }
   }
  }
  if(!$hit){& clang @common -c $source -o $object;if($LASTEXITCODE){throw "Bluetooth compile failed: $relative"};$script:compiled+=$relative}
  $script:records+=@{source=$relative;source_sha256=$sourceHash;object=[IO.Path]::GetFileName($object);sha256=(Get-FileHash $object -Algorithm SHA256).Hash;dependencies=$deps}
 }
 $objects=@();$sources=@()
 foreach($name in @('transport','sideband','status','hfp_target','windows_driver')){
  $obj=Join-Path $out "$name.obj";$objects+=$obj
  $source="$root\drivers\bluetooth\mt6630\$name.c"
  $deps=@("drivers/bluetooth/mt6630/$name.h")
  if($name -eq 'transport'){$deps+=$networkHeaders}
  if($name -eq 'windows_driver'){$deps=@('transport.h','status.h','hfp_target.h','hfp_controller_contract.h')|ForEach-Object {"drivers/bluetooth/mt6630/$_"};$deps+=$networkHeaders}
  if($name -eq 'status'){$deps+='tools/bluetooth/mt6630-status.h'}
  Build-Unit "drivers/bluetooth/mt6630/$name.c" $obj $deps
  $sources+=@{path="drivers/bluetooth/mt6630/$name.c";sha256=(Get-FileHash $source -Algorithm SHA256).Hash}
 }
 # An explicit set; do not link stale objects from another driver invocation.
 foreach($name in @('windows_irq','windows_nvram_reader','windows_firmware','sha256','windows_board','resource_lease','bringup','board_sequence','msdc2_pio','power_bridge','power_plan','transport','direct','calibration','nvram_sector','wlan_firmware','windows_msdc2','lifetime','identity','enumerate','control_start','native_tx','regulatory','wmt_bridge')){
  $obj=Join-Path $out ("common-$name.obj");$objects+=$obj
  $source="$root\drivers\network\mt6630\$name.c"
  Build-Unit "drivers/network/mt6630/$name.c" $obj $networkHeaders
  $sources+=@{path="drivers/network/mt6630/$name.c";sha256=(Get-FileHash $source -Algorithm SHA256).Hash}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref $objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt6630-bluetooth.sys"
 if($LASTEXITCODE){throw 'MT6630 Bluetooth SYS link failed'}
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-bluetooth.inf" $out -Force
 $transportInf=Get-Content (Join-Path $out 'mt6630-bluetooth.inf') -Raw
 if($transportInf -notmatch '(?m)^DriverVer=([^\r\n]+)'){throw 'Transport INF DriverVer missing'}
 $transportDriverVer=$Matches[1]
 $headers=@(Get-ChildItem "$root\drivers\bluetooth\mt6630","$root\drivers\network\mt6630" -File -Recurse -Filter '*.h'|ForEach-Object{@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 @{source_commit=$commit;source_dirty=$false;sources=$sources;headers=$headers;object_cache_version=1;clang_sha256=$clangHash;sdk_headers_sha256=$sdkHash;reuse_manifest_sha256=$ReuseManifestSha256;object_records=$records;compiled_units=$compiled;reused_units=$reused;kernel_compile_flags=$common;kernel_codegen_flags=$kernelCodegenFlags;
   crt_library_sha256=(Get-FileHash "$km\libcntpr.lib" -Algorithm SHA256).Hash;transport_monotonic_deadlines=$true;hfp_controller_registration_version=2;hfp_controller_parent_v2_required=$true;hfp_controller_arrival_interface=$true;hfp_target_match="PnP Parent/Children properties; exact dynamic devnode IDs";hfp_pcm_qualified=$false;hci_actual_sco_events_tracked=$true;transport_off_ack_tx_rx_partial_drain=$true;wdf_read_queues_joined=$true;afe_dma_drain_proven=$false;hfp_codec_latency_known=$false;required_parent_bt_profile=1;shared_parent_built=[bool]$BuildSharedParent;shared_parent_reused=[bool]$ReuseParentDirectory;
   parent_inf=$parentInf;parent_source_commit=$parentSource;parent_sys_sha256=$parentHash;parent_manifest_sha256=$parentManifestHash;
   transport_driver_ver=$transportDriverVer;transport_inf_sha256=(Get-FileHash (Join-Path $out 'mt6630-bluetooth.inf') -Algorithm SHA256).Hash;
   bthx_device_control=$true;bthx_parent_queue_forwarding=$true;standard_bluetooth_verified=$false;
   physical_transport='MSDC2 SDIO function 2 / STP task 0';radio_profile='exclusive Bluetooth; WLAN PDO absent';
   firmware_embedded=$false;individual_nvram_embedded=$false;hardware_verified=$false;wdk_link_succeeded=$true;
   sco_support='None; documented Windows 8/8.1 BTHX load contract not satisfied';
   advertised_sco=0;advertised_max_sco_channels=0;required_sco='HCIBypass';required_max_sco_channels=1;
   wmt_merge_interface=1;sideband_core='MRG narrowband CVSD 8k mono S16; owner integration pending';sideband_audio_provider_connected=$false;
   standard_stack_contract_satisfied=$false;standard_stack_load_candidate=$false;bthx_contract_query_abi=1;
   sys_sha256=(Get-FileHash (Join-Path $out 'mt6630-bluetooth.sys') -Algorithm SHA256).Hash
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 $manifest
} finally {foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}
