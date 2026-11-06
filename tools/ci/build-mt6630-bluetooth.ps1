# WDK8.1 ARM32: exclusive Bluetooth radio profile; no signing/deployment/runner.
param([string]$OutputDirectory,[switch]$BuildSharedParent)
$ErrorActionPreference='Stop'
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
 if($BuildSharedParent){
  # Reuse the full Audio v11 + USB + input parent, changing just its single
  # MSDC2 child profile. Never create a second physical host PDO.
  & "$PSScriptRoot\build-mt8167-first-playback.ps1" -IntegratedUsb -BluetoothProfile -OutputDirectory (Join-Path $out 'audio-usb')
  $parentManifest=Get-Content (Join-Path $out 'audio-usb\manifest.json') -Raw|ConvertFrom-Json
  if($parentManifest.diagnostic_interface -ne 11 -or !$parentManifest.integrated_usb -or
     $parentManifest.parent_flags.A04_SHARED_BT_PROFILE -ne '1' -or
     $parentManifest.parent_flags.A04_SHARED_WIFI_RUNTIME -ne '1'){
    throw 'Bluetooth retained parent profile mismatch'
  }
 }
 & "$PSScriptRoot\build-mt6630-core.ps1"
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $km=Join-Path $kit 'Lib\winv6.3\km\arm';$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
 $objects=@()
 foreach($name in @('transport','status','windows_driver')){
  $obj=Join-Path $out "$name.obj";$objects+=$obj
  & clang --target=thumbv7-pc-windows-msvc -std=c11 -w -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_WIFI_IRQ_DIAGNOSTIC=1 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c "$root\drivers\bluetooth\mt6630\$name.c" -o $obj
  if($LASTEXITCODE){throw "Bluetooth $name compile failed"}
 }
 # An explicit set; do not link stale objects from another driver invocation.
 foreach($name in @('windows_irq','windows_nvram_reader','windows_firmware','sha256','windows_board','resource_lease','bringup','board_sequence','msdc2_pio','power_bridge','power_plan','transport','direct','calibration','nvram_sector','wlan_firmware','windows_msdc2','lifetime','identity','enumerate','control_start','native_tx','regulatory','wmt_bridge')){
  $objects+=Join-Path (Join-Path $env:TEMP 'a04br3-mt6630-core') "$name.obj"
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref $objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt6630-bluetooth.sys"
 if($LASTEXITCODE){throw 'MT6630 Bluetooth SYS link failed'}
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-bluetooth.inf" $out -Force
 @{source_commit=$commit;source_dirty=$false;required_parent_bt_profile=1;shared_parent_built=[bool]$BuildSharedParent;
   physical_transport='MSDC2 SDIO function 2 / STP task 0';radio_profile='exclusive Bluetooth; WLAN PDO absent';
   firmware_embedded=$false;individual_nvram_embedded=$false;hardware_verified=$false;wdk_link_succeeded=$true;
   sco_support='None (unverified wiring; BthMini acceptance remains unverified)';
   sys_sha256=(Get-FileHash (Join-Path $out 'mt6630-bluetooth.sys') -Algorithm SHA256).Hash
 }|ConvertTo-Json|Set-Content -Encoding UTF8 $manifest
} finally {foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}
