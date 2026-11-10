# 実fresh200専用・private clone read-only inventory。profile/native/capture-contractは事前不要。
param([Parameter(Mandatory=$true)][string]$WorkspaceRoot,
 [Parameter(Mandatory=$true)][string]$Manifest,[Parameter(Mandatory=$true)][string]$ExpectedManifestSha256,
 [Parameter(Mandatory=$true)][string]$SourceDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$PolicyPath,[Parameter(Mandatory=$true)][string]$ExpectedPolicyFileSha256,
 [string]$ExpectedPolicySha256,[ValidatePattern('^MT8167CombinedFollowup[0-9]{3}$')][string]$NewServiceName='MT8167CombinedFollowup200',
 [string[]]$AdditionalServiceNames=@(),[int]$TimingCaptureNumber=0,[switch]$IncludeWifiCheckpoint,[switch]$IncludeGraphicsTopologyCache,[switch]$IncludeDisplayDeviceBindings,[switch]$IncludeBootEnvironment,[switch]$IncludeBatteryFirstFailure)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$w=[IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
foreach($item in @($Manifest,$SourceDirectory,$OutputDirectory,$PolicyPath)){
 if(![IO.Path]::GetFullPath($item).StartsWith($w+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Private inputs/output must stay in workspace'}
}
if(!$ExpectedPolicySha256){$ExpectedPolicySha256=$ExpectedPolicyFileSha256}
foreach($h in @($ExpectedManifestSha256,$ExpectedPolicyFileSha256,$ExpectedPolicySha256)){if($h -cnotmatch '^[0-9a-f]{64}$'){throw 'Explicit SHA256 required'}}
if($TimingCaptureNumber){
 if($TimingCaptureNumber -lt 204 -or $TimingCaptureNumber -gt 999){throw 'Bounded timing capture204..999 required'}
 if($PSBoundParameters.ContainsKey('NewServiceName') -and $NewServiceName -cne "MT8167CombinedFollowup$TimingCaptureNumber"){throw 'New service/capture mismatch'}
 $NewServiceName="MT8167CombinedFollowup$TimingCaptureNumber"
 if((Hash $Manifest) -cne $ExpectedManifestSha256 -or (Hash $PolicyPath) -cne $ExpectedPolicyFileSha256){throw 'Explicit actual fresh/template SHA mismatch'}
 $m=Get-Content -LiteralPath $Manifest -Raw|ConvertFrom-Json
 $p=Get-Content -LiteralPath $PolicyPath -Raw|ConvertFrom-Json
 if($Manifest.Replace('\','/') -cnotmatch ("/private-windows-rt-prebackup-run"+$TimingCaptureNumber+"/manifest.json$")){throw 'Actual capture path mismatch'}
 if($m.status -cne 'captured-unmounted' -or $m.read_only -ne $true -or $m.unmounted -ne $true -or $m.errors.Count -or !$m.recovery_boot_id){throw 'Completed real fresh capture required'}
 if(!$p.service_preservation -or !$p.captured_registry_values -or @($p.service_preservation.PSObject.Properties).Count -gt 128 -or @($p.captured_registry_values).Count -gt 2048){throw 'Bounded actual template names required'}
}else{
 if($NewServiceName -cne 'MT8167CombinedFollowup200'){throw 'Legacy200 service identity required'}
if($ExpectedManifestSha256 -cne 'fbba59d9561ca61f9ff1b41e6bbe0a451330b91741abf487a5caaeeb420b2ed9' -or (Hash $Manifest) -cne $ExpectedManifestSha256 -or (Hash $PolicyPath) -cne $ExpectedPolicyFileSha256){throw 'Actual immutable fresh200/template pin mismatch'}
$m=Get-Content -LiteralPath $Manifest -Raw|ConvertFrom-Json
$p=Get-Content -LiteralPath $PolicyPath -Raw|ConvertFrom-Json
if($m.recovery_boot_id -cne '64657ae7-390c-431b-ae4d-ae5de209793b' -or $m.status -cne 'captured-unmounted' -or $m.read_only -ne $true -or $m.unmounted -ne $true -or $m.errors.Count){throw 'Completed real fresh200 required'}
if($p.profile -cne 'combined-features-fresh196-v1' -or !$p.service_preservation -or !$p.captured_registry_values){throw 'Canonical196 template supplies names only; not new snapshot'}
}
if(Test-Path -LiteralPath $OutputDirectory){throw 'New private output only'}
if('MT8167OfflineHive.Native' -as [type]){throw 'Use a fresh Windows PowerShell process for the isolated clone reader'}
foreach($name in $AdditionalServiceNames){if($name -cnotmatch '^[A-Za-z0-9_-]{1,100}$' -or $name -ceq $NewServiceName){throw 'Bounded additional retained service name required'}}
$pins=[ordered]@{}
foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){
 $rows=@($m.files|Where-Object {$_.source_path -ceq "Windows/System32/config/$name"})
 $file=Join-Path $SourceDirectory $name
 if($rows.Count -ne 1 -or (Hash $file) -cne $rows[0].sha256 -or (Get-Item -LiteralPath $file).Length -ne $rows[0].bytes){throw 'Original trio pin/size mismatch'}
 $pins[$name]=$rows[0].sha256
}
$capturedDrivers=[ordered]@{}
if($IncludeWifiCheckpoint){
 $driverDirectory=Join-Path (Split-Path ([IO.Path]::GetFullPath($SourceDirectory))) 'Drivers'
 foreach($name in @('ge8300-render.sys','mt8167-display-transport.sys','mt6630-combo-runtime.sys')){
  $rows=@($m.files|Where-Object {$_.source_path -ceq "Windows/System32/Drivers/$name"})
  $file=Join-Path $driverDirectory $name
  if($rows.Count -ne 1 -or (Hash $file) -cne $rows[0].sha256 -or (Get-Item -LiteralPath $file).Length -ne $rows[0].bytes){throw 'Captured GPU/WiFi SYS pin/size mismatch'}
  $capturedDrivers[$name]=[ordered]@{source_path=$rows[0].source_path;sha256=$rows[0].sha256;bytes=$rows[0].bytes;capture_file_verified=$true;loaded_image_verified=$false}
 }
}
New-Item -ItemType Directory "$OutputDirectory\clone"|Out-Null
foreach($name in $pins.Keys){Copy-Item -LiteralPath (Join-Path $SourceDirectory $name) -Destination "$OutputDirectory\clone\$name"}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
New-Item -ItemType Directory "$OutputDirectory\compiler-temp"|Out-Null
try{$env:TEMP="$OutputDirectory\compiler-temp";$env:TMP=$env:TEMP;Add-Type -Path "$PSScriptRoot\ReadonlyInventory200HiveNative.cs"}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
$t=$null;$parseErrors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\prepare-offline-system-service.ps1",[ref]$t,[ref]$parseErrors)
if($parseErrors.Count){throw 'Production snapshot source parse failed'}
$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-ServiceSnapshot'},$true))
if($f.Count -ne 1){throw 'One production subtree snapshot function required'}
Invoke-Expression $f[0].Extent.Text
function Values($k,[string[]]$only,[int]$limit=64){
 # Native.OpenPath uses RegistryKey.FromHandle; its Name is not a path authority.
 # Only the fixed FactoryCheckpoint call site supplies90 and R00..R69 names.
 if($limit -notin @(64,90) -or ($limit -eq 90 -and @($only|Where-Object{$_ -cnotmatch '^R[0-6][0-9]$'}).Count)){throw 'Explicit FactoryCheckpoint90 bound required'}
 $r=[ordered]@{};$names=@($k.GetValueNames());if($names.Count -gt $limit -or $k.GetSubKeyNames().Count -gt 32){throw 'Fixed key bound exceeded'}
 foreach($n in $only){if($n -cnotin $names){$r[$n]=@{present=$false};continue};$kind=[string]$k.GetValueKind($n);if($kind -cnotin @('String','ExpandString','MultiString','Binary','DWord','QWord','None')){throw 'Unknown fixed value type'};$v=$k.GetValue($n,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);if($v -is [byte[]]){if($v.Length -gt 65536){throw 'Value byte bound exceeded'};$v=[BitConverter]::ToString($v).Replace('-','')}elseif($v -is [string]){if($v.Length -gt 32767){throw 'String byte bound exceeded'}}elseif($v -is [string[]]){if(2*(($v -join '').Length+$v.Count+1) -gt 65536){throw 'MultiString byte bound exceeded'}};$r[$n]=@{present=$true;kind=$kind;value=$v}}
 return $r
}
# Additive fixed-leaf runtime observation. This is never a preservation bypass.
function Keypad-WdfRuntimeObservation($Root) {
 $k=[MT8167OfflineHive.Native]::OpenPath($Root,'ControlSet001\Services\Mt6392Keys\Parameters\Wdf',$false)
 if(!$k){return [ordered]@{present=$false}}
 try {
  if($k.GetValueNames().Count -gt 64 -or $k.GetSubKeyNames().Count -gt 32){throw 'Keypad WDF observation bound exceeded'}
  $name='TimeOfLastSqmLog';$v=[ordered]@{present=$false}
  if($name -cin $k.GetValueNames()){
   if($k.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::QWord){throw 'Keypad WDF timestamp must remain REG_QWORD'}
   $v=[ordered]@{present=$true;type=11;bytes=8;raw_hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,$name,11,8);interpretation='WDF namespaced runtime timestamp; not static configuration or proof of writer'}
  }
  return [ordered]@{present=$true;path='ControlSet001\Services\Mt6392Keys\Parameters\Wdf';timestamp=$v;strict_full_fingerprint_check_retained=$true}
 }finally{$k.Dispose()}
}
$r=[ordered]@{schema=$(if($TimingCaptureNumber){"GPU369_FRESH$($TimingCaptureNumber)_READONLY_NATIVE_RECEIPT_V1"}else{'GPU368_FRESH200_READONLY_NATIVE_RECEIPT_V1'});scope='Actual private clone inventory; no previous snapshot comparison and no hardware execution';capture_number=$(if($TimingCaptureNumber){$TimingCaptureNumber}else{200});source_manifest_sha256=$ExpectedManifestSha256;recovery_boot_id=$m.recovery_boot_id;template_policy_sha256=$ExpectedPolicyFileSha256;policy_sha256=$ExpectedPolicySha256;policy_sha256_is_reference_only=$true;source_files=$pins;new_service_name=$NewServiceName;nodes=[ordered]@{};old_services=[ordered]@{};captured_named_values=[ordered]@{};missing_named_values=@();actual_driver_services=[ordered]@{};full_service_preservation_equal=$null;preservation_comparison_performed=$false;full_service_capture_complete=$false;combined=[ordered]@{};errors=@();load_rc=-1;unload_rc=-1;privilege_restore_error=-1;originals_unchanged=$false;physical_boot_read_performed=$false;registry_write_requested=$false;clone_replay_may_modify_clone=$true;hardware_verified=$false}
$r.captured_driver_files=$capturedDrivers
$r.source_recipe_files=[ordered]@{}
foreach($file in @('read-fresh200-native-inventory.ps1','ReadonlyInventory200HiveNative.cs','prepare-offline-system-service.ps1','read-post-ownership-policy360.ps1','read-gpu-native-checkpoint.ps1','read-ge8300-start-gap359.ps1')){$r.source_recipe_files[$file]=Hash (Join-Path $PSScriptRoot $file)}
if($IncludeWifiCheckpoint){$r.source_recipe_files['read-mt6630-checkpoint-loaded.ps1']=Hash (Join-Path $PSScriptRoot 'read-mt6630-checkpoint-loaded.ps1')}
if($IncludeBootEnvironment){$r.source_recipe_files['read-boot-environment.ps1']=Hash (Join-Path $PSScriptRoot 'read-boot-environment.ps1')}
if($IncludeGraphicsTopologyCache){$r.source_recipe_files['read-graphics-topology-cache.ps1']=Hash (Join-Path $PSScriptRoot 'read-graphics-topology-cache.ps1')}
if($IncludeDisplayDeviceBindings){$r.source_recipe_files['read-display-device-bindings.ps1']=Hash (Join-Path $PSScriptRoot 'read-display-device-bindings.ps1')}
if($IncludeBatteryFirstFailure){$r.source_recipe_files['read-lc709203f-first-failure.ps1']=Hash (Join-Path $PSScriptRoot 'read-lc709203f-first-failure.ps1')}
$wifiCheckpoint=$null
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
try{
 $priv=New-Object MT8167OfflineHive.Privileges
 $r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\clone\SYSTEM");if($r.load_rc){throw 'Private clone load failed'}
 $loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false)
 if(!$select){throw 'Select absent'}
 try{$r.select=Values $select @('Current','Default','LastKnownGood');$current=$select.GetValue('Current');if($current -ne 1){throw 'Explicit ControlSet001 only'}}finally{$select.Dispose()}
 $cs='ControlSet001'
 . "$PSScriptRoot\read-post-ownership-policy360.ps1"
 $r.post_ownership_policy360=Read-PostOwnershipPolicy360 $root
 # These are observed fresh values, not a comparison against the old template.
 $names=@($p.service_preservation.PSObject.Properties.Name)+@($AdditionalServiceNames)
 $services=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services",$false)
 if(!$services){throw 'Services absent'}
 try{
  $all=@($services.GetSubKeyNames());if($all.Count -gt 4096){throw 'Service namespace bound exceeded'}
  $names+=@($all|Where-Object {($_ -cmatch '^MT8167CombinedFollowup[0-9]{3}$' -or $_ -cmatch '^MT8167Usb1121[A-Za-z0-9_-]{0,80}$') -and $_ -cne $NewServiceName})
 }finally{$services.Dispose()}
 $names=@($names|Sort-Object -Unique)
 if(!$names.Count -or $names.Count -gt 128){throw 'Retained service inventory bound exceeded'}
 foreach($name in $names){
  if($name -cnotmatch '^[A-Za-z0-9_-]{1,100}$' -or $name -ceq $NewServiceName){throw 'Invalid retained service name'}
  $r.old_services[$name]=Peripheral-ServiceSnapshot $root "$cs\Services\$name"
 }
 $requests=[ordered]@{}
 foreach($v in $p.captured_registry_values){$id=$v.key+'\'+$v.name;if($requests.Contains($id)){throw 'Duplicate template value'};$requests[$id]=@{key=$v.key;name=$v.name;required=$true}}
 # Discover actual root values for newer retained SCM services, including 196/USB198.
 foreach($name in $names){
  if($name -cnotmatch '^MT8167(CombinedFollowup[0-9]{3}|Usb1121[A-Za-z0-9_-]{0,80})$' -or !$r.old_services[$name].present){continue}
  foreach($vn in $(if($TimingCaptureNumber){@('Start','ImagePath','Type','ObjectName','DependOnService','ErrorControl','DisplayName')}else{@('Start','ImagePath','Type','ObjectName','DependOnService')})){
   $key='SYSTEM\CurrentControlSet\Services\'+$name;$id=$key+'\'+$vn
   if(!$requests.Contains($id)){$requests[$id]=@{key=$key;name=$vn;required=$false}}
  }
 }
 if($requests.Count -gt 2048){throw 'Named-value inventory bound exceeded'}
 foreach($id in $requests.Keys){
  $v=$requests[$id]
  # Actual canonical template positions5..9: existing BT ROOT/SYSTEM/0001.
  # This is one fixed enum leaf and five typed values, not a general Enum allowance.
  $btRootTypes=@{Service=1;ClassGUID=1;Driver=1;HardwareID=7;ConfigFlags=4}
  $fixedBtRoot=$v.key -ceq 'SYSTEM\CurrentControlSet\Enum\ROOT\SYSTEM\0001'
  $serviceValue=$v.key.StartsWith('SYSTEM\CurrentControlSet\Services\',[StringComparison]::Ordinal)
  if((!$serviceValue -and !$fixedBtRoot) -or !$v.name -or $v.name.Length -gt 255 -or ($fixedBtRoot -and !$btRootTypes.ContainsKey($v.name))){throw 'Bounded named service or exact BT root value required'}
  $relative=$v.key.Replace('SYSTEM\CurrentControlSet\',$cs+'\');$key=[MT8167OfflineHive.Native]::OpenPath($root,$relative,$false)
  $value=$null;try{if($key){$value=[MT8167OfflineHive.Native]::ReadRawValue($key,$v.name,4096)}}finally{if($key){$key.Dispose()}}
  if(!$value -or !$value.Present){if($v.required){$r.missing_named_values+=@($id)};continue}
  if($value.Type -notin @(1,2,3,4,7,11) -or $value.Bytes -le 0){throw 'Unsupported captured named value type/size'}
  if($fixedBtRoot -and $value.Type -ne $btRootTypes[$v.name]){throw 'Exact BT root named-value type mismatch'}
  $r.captured_named_values[$id]=@{type=[int]$value.Type;bytes=$value.Bytes;raw_hex=$value.RawHex;present=$true}
 }
 if($r.missing_named_values.Count){throw 'Required template names are missing in actual fresh200; keep receipt, do not synthesize values'}
 $r.keypad_wdf_runtime=Keypad-WdfRuntimeObservation $root
 $failed=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167CombinedFollowup148",$false)
 if($failed){$failed.Dispose();throw 'Rejected149 service unexpectedly exists'};$r.rejected149_service_absent=$true
 $svc=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\$NewServiceName",$false)
 if(!$svc){$r.new_service=@{present=$false;name=$NewServiceName}}else{try{$r.new_service=@{present=$true;name=$NewServiceName;values=(Values $svc @('Start','ImagePath','Type','ObjectName','DependOnService'))}}finally{$svc.Dispose()}}
 foreach($id in @('ACPI\BEN0001','MT8167\AFE','ACPI\MOU0001','MT8167\USB20','MT8167\USBPORT20','A04BR3\MT6630SDIO','ACPI\MOU0007','ACPI\MOU0006','ACPI\MOU0010','ACPI\MOU0013','MT6392\RTC','MT8167\Thermal','MT6392\Buttons','ACPI\MOU0008')){
  $k=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\$id",$false);if(!$k){$r.nodes[$id]=@{present=$false};continue}
  try{$rows=@();$names=@($k.GetSubKeyNames());if($names.Count -gt 8){throw 'Fixed node instance bound exceeded'};foreach($n in $names){$leaf=[MT8167OfflineHive.Native]::OpenPath($root,("$cs\Enum\$id\"+$n),$false);try{$v=Values $leaf @('Service','ClassGUID','Driver','HardwareID','ConfigFlags','LowerFilters','UpperFilters');$row=[ordered]@{instance=$n;values=$v;selected_present=$false};if($v.Driver.present){$d=[string]$v.Driver.value;if($d -cnotmatch '^\{[a-fA-F0-9-]{36}\}\\[0-9]{4}$'){throw 'Unknown selected class path'};$dk=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Control\Class\$d",$false);if($dk){try{$row.selected=Values $dk @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName','LowerFilters','UpperFilters');$row.selected_present=$true}finally{$dk.Dispose()}}};$rows+=@($row)}finally{$leaf.Dispose()}};$r.nodes[$id]=@{present=$true;instances=$rows}}finally{$k.Dispose()}
 }
 $matches=@();$rk=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\ROOT",$false)
 if($rk){try{$devices=@($rk.GetSubKeyNames());if($devices.Count -gt 256){throw 'ROOT device bound exceeded'};foreach($device in $devices){$dk=[MT8167OfflineHive.Native]::OpenPath($root,("$cs\Enum\ROOT\"+$device),$false);try{$instances=@($dk.GetSubKeyNames());if($instances.Count -gt 32){throw 'ROOT instance bound exceeded'};foreach($instance in $instances){$path="$cs\Enum\ROOT\"+$device+'\'+$instance;$leaf=[MT8167OfflineHive.Native]::OpenPath($root,$path,$false);try{$hw=$leaf.GetValue('HardwareID');if($hw -and @($hw|Where-Object{$_ -ieq 'ROOT\MT6630COMBO'}).Count){$matches+=@(@{instance=($device+'\'+$instance);registry_path=$path;values=(Values $leaf @('Service','ClassGUID','Driver','HardwareID','CompatibleIDs','ConfigFlags'))})}}finally{$leaf.Dispose()}}}finally{$dk.Dispose()}}}finally{$rk.Dispose()}}
 if($matches.Count -gt 1){throw 'Duplicate BT root hardware identity'}
 $r.nodes['ROOT\MT6630COMBO']=@{present=($matches.Count -eq 1);instances=$matches;selection='exact hardware-ID match including generated ROOT/System instances'}
 # BT selection is read from the actual hardware-ID matched ROOT instance above;
 # RegistryKey.Name from a raw handle is not used as a path authority.
 $bt=$r.nodes['ROOT\MT6630COMBO'];$r.bt_start=@{selection=$null;selection_source='actual hardware-ID matched class leaf';hardware_success_inferred=$false}
 if($bt.present -and @($bt.instances).Count -eq 1){
  $inst=$bt.instances[0];$d=$inst.values.Driver.value
  if($d -cmatch '^\{[a-fA-F0-9-]{36}\}\\[0-9]{4}$'){
   $key=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Control\Class\$d",$false)
   if($key){try{$r.bt_start.selection=Values $key @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName','LowerFilters','UpperFilters')}finally{$key.Dispose()}}
  }
 }
 foreach($row in $r.nodes.Values){
  foreach($inst in @($row.instances)){
   if($inst.values.Service.present){
    $name=[string]$inst.values.Service.value
    if($name -cmatch '^[A-Za-z0-9_-]{1,100}$' -and !$r.actual_driver_services.Contains($name)){
     $driverKey=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\$name",$false)
     if(!$driverKey){$r.actual_driver_services[$name]=@{present=$false}}else{try{$r.actual_driver_services[$name]=@{present=$true;root_sha256=[MT8167OfflineHive.Native]::ValueFingerprint($driverKey);values=(Values $driverKey @('Start','ImagePath','Type','ObjectName','DependOnService','Group'));subtree_fingerprint_claimed=$false}}finally{$driverKey.Dispose()}}
    }
   }
  }
 }
 $key=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\GE8300Render\Parameters\FactoryCheckpoint",$false)
 if(!$key){$r.gpu_factory=@{present=$false}}else{try{$fields=@($key.GetValueNames()|Where-Object {$_ -cmatch '^R[0-6][0-9]$'});$r.gpu_factory=@{present=$true;values=(Values $key $fields 90);timestamps_retained=$true;records_not_assumed_current_boot=$true}}finally{$key.Dispose()}}
 . "$PSScriptRoot\read-gpu-native-checkpoint.ps1"
 $r.gpu_init=Read-GpuNativeCheckpoint $root $cs
 . (Join-Path $PSScriptRoot 'read-ge8300-start-gap359.ps1')
 $r.startup_gap359=Read-Ge8300StartGap359 $root
 if($IncludeGraphicsTopologyCache){
  . (Join-Path $PSScriptRoot 'read-graphics-topology-cache.ps1')
  $r.graphics_topology_cache=Read-GraphicsTopologyCache $root $cs
  if($r.graphics_topology_cache.errors.Count){$r.errors+=@('Bounded GraphicsDrivers cache observation failed; see graphics_topology_cache.errors')}
 }
 if($IncludeDisplayDeviceBindings){
  . (Join-Path $PSScriptRoot 'read-display-device-bindings.ps1')
  $r.display_device_bindings=Read-DisplayDeviceBindings $root $cs
  if($r.display_device_bindings.errors.Count){$r.errors+=@('Bounded display bindings failed; see display_device_bindings.errors')}
 }
 if($IncludeWifiCheckpoint){
  . (Join-Path $PSScriptRoot 'read-mt6630-checkpoint-loaded.ps1')
  $wifiCheckpoint=Read-Mt6630LoadedCheckpoint $root $cs "$OutputDirectory\wifi-checkpoint" $ExpectedManifestSha256 $m.recovery_boot_id
  $r.wifi_checkpoint=[ordered]@{receipt_relative_path='wifi-checkpoint/receipt.json';selection=$wifiCheckpoint.selection;record_headers=$wifiCheckpoint.record_headers;values=$wifiCheckpoint.values;same_loaded_root=$true;connection_success_claimed=$false}
 }
 $key=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\GE8300Render\Parameters\NativeCheckpoint",$false)
 $r.module_identity=@{present=$false}
 if($key){try{
  $raw=[MT8167OfflineHive.Native]::ReadRawValue($key,'ModuleIdentityV1',144)
  if($raw.Present){
   if($raw.Type -ne 3 -or $raw.Bytes -ne 144){throw 'ModuleIdentityV1 type/size mismatch'}
   $bytes=New-Object byte[] 144;for($i=0;$i -lt 144;$i++){$bytes[$i]=[Convert]::ToByte($raw.RawHex.Substring($i*2,2),16)}
   [IO.File]::WriteAllBytes("$OutputDirectory\module-identity.bin",$bytes)
   $source=[Text.Encoding]::ASCII.GetString($bytes,48,48).TrimEnd([char]0);$build=[Text.Encoding]::ASCII.GetString($bytes,96,48).TrimEnd([char]0)
   $r.module_identity=@{present=$true;bytes=144;sha256=(Hash "$OutputDirectory\module-identity.bin");observed_sourcebase=$source;observed_buildid=$build;expected_sourcebase='e0727eb0d840883a31d3c501b0ee17652ebdcad4';expected_buildid='GPU359-start-gap-v1';expected_identity_matches=($source -ceq 'e0727eb0d840883a31d3c501b0ee17652ebdcad4' -and $build -ceq 'GPU359-start-gap-v1');reference_only=$true;same_boot_correlation_required=$true}
  }
 }finally{$key.Dispose()}}
 $r.observed_counts=@{old_services=$r.old_services.Count;named_values=$r.captured_named_values.Count;nodes=$r.nodes.Count}
 $r.full_service_capture_complete=$true
 if($IncludeBatteryFirstFailure){
  . (Join-Path $PSScriptRoot 'read-lc709203f-first-failure.ps1')
  $r.battery_first_failure=Read-Lc709203fFirstFailure $root $cs
 }
 if($IncludeBootEnvironment){
  . (Join-Path $PSScriptRoot 'read-boot-environment.ps1')
  $r.boot_environment=Read-BootEnvironment $root $cs
 }
 if($r.new_service.present){throw 'NewServiceName200 is actually present; do not arm or mislabel it absent'}
}catch{$r.errors+=@($_.Exception.Message)}finally{
 if($root){$root.Dispose()}
 if($loaded){try{$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)}catch{$r.errors+=@("Unload exception: "+$_.Exception.Message)}}
 if($priv){try{$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}catch{$r.errors+=@("Privilege restore exception: "+$_.Exception.Message)}}
}
$r.originals_unchanged=$true
foreach($name in $pins.Keys){if((Hash (Join-Path $SourceDirectory $name)) -cne $pins[$name]){$r.originals_unchanged=$false;$r.errors+=@('Original trio changed: '+$name)}}
foreach($name in $capturedDrivers.Keys){if((Hash (Join-Path $driverDirectory $name)) -cne $capturedDrivers[$name].sha256){$r.errors+=@('Captured SYS original changed: '+$name)}}
$r.manifest_unchanged=(Hash $Manifest) -ceq $ExpectedManifestSha256
$r.template_unchanged=(Hash $PolicyPath) -ceq $ExpectedPolicyFileSha256
if(!$r.manifest_unchanged -or !$r.template_unchanged){$r.errors+=@('Input manifest/template changed')}
if($null -ne $wifiCheckpoint){
 foreach($field in @('load_rc','unload_rc','privilege_restore_error','originals_unchanged','manifest_unchanged')){$wifiCheckpoint[$field]=$r[$field]}
 $wifiCheckpoint.errors=@($r.errors)
 $clean=(!$r.errors.Count -and $r.load_rc -eq 0 -and $r.unload_rc -eq 0 -and $r.privilege_restore_error -eq 0 -and $r.originals_unchanged -and $r.manifest_unchanged)
 $wifiCheckpoint.status=if($clean){'checkpoint-extracted-not-a-connection-diagnosis'}else{'failed'}
 $wifiCheckpoint|ConvertTo-Json -Depth 25|Set-Content -LiteralPath "$OutputDirectory\wifi-checkpoint\receipt.json" -Encoding UTF8
}
$r|ConvertTo-Json -Depth 30|Set-Content -LiteralPath "$OutputDirectory\receipt.json" -Encoding UTF8
if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.originals_unchanged){throw 'Read-only inventory failed; retain receipt and clone'}
'FRESH200_ACTUAL_NATIVE_INVENTORY_CAPTURED_NO_ARMING_OR_HARDWARE_SUCCESS'
