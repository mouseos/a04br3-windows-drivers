# Fixed read-only host clone reader. No SDK, staging, live device or original hive load.
param([Parameter(Mandatory=$true)][string]$WorkspaceRoot,[Parameter(Mandatory=$true)][string]$Manifest,
 [Parameter(Mandatory=$true)][string]$ExpectedManifestSha256,[Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$PolicyPath,
 [Parameter(Mandatory=$true)][string]$ExpectedPolicyFileSha256,[Parameter(Mandatory=$true)][string]$ExpectedPolicySha256)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Hash($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$w=[IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
foreach($p in @($Manifest,$SourceDirectory,$OutputDirectory,$PolicyPath)){if(![IO.Path]::GetFullPath($p).StartsWith($w+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Workspace-local private paths required'}}
foreach($h in @($ExpectedManifestSha256,$ExpectedPolicyFileSha256,$ExpectedPolicySha256)){if($h -cnotmatch '^[0-9a-f]{64}$'){throw 'Full explicit hash required'}}
if((Hash $Manifest) -cne $ExpectedManifestSha256 -or (Hash $PolicyPath) -cne $ExpectedPolicyFileSha256){throw 'Input manifest/policy byte pin mismatch'}
. "$PSScriptRoot\read-ge8300-start357.ps1"
$m=Get-Content $Manifest -Raw|ConvertFrom-Json;$p=Get-Content $PolicyPath -Raw|ConvertFrom-Json;$readerClientCountSchema=@($p.diagnostic_clients).Count;if($readerClientCountSchema -ne 2 -or $p.gpu_client_execute -ne $true -or $p.gpu_client_count -ne 2 -or $p.bt_first_install_execute -ne $false -or $p.bt_readonly_execute -ne $false -or $p.diagnostic_clients[0].name -cne 'gpu_present' -or $p.diagnostic_clients[0].phase -ne 1 -or $p.diagnostic_clients[1].name -cne 'gpu_globaldiag' -or $p.diagnostic_clients[1].phase -ne 1){throw 'Exact GPU367 idx0 only contract required'}
if($p.module_identity_collection.expected_buildid -cne 'GPU359-start-gap-v1' -or $p.module_identity_collection.expected_sourcebase -cne 'e0727eb0d840883a31d3c501b0ee17652ebdcad4' -or $p.baseline200_module_identity_reference_only.observed_buildid -cne 'GPU359-start-gap-v1'){throw 'Post198 GPU359 and fresh200 GPU359 baseline must remain separate'}
$serviceNames=@($p.fresh200_inventory.service_names);if($serviceNames.Count -ne $p.fresh200_inventory.service_count -or @($p.captured_registry_values).Count -ne $p.fresh200_inventory.named_value_count){throw 'Exact captured fresh200 inventory required'}
if((@($p.service_preservation.PSObject.Properties.Name|Sort-Object) -join ',') -cne ((@($serviceNames)|Sort-Object) -join ',')){throw 'Exact eighteen old service snapshots required'}
if($m.status -cne 'captured-unmounted' -or !$m.unmounted -or !$m.read_only -or $m.errors.Count -or $p.profile -cne 'combined-features-fresh200-gpu368-v1' -or $p.service -cne 'MT8167CombinedFollowup200' -or $p.data_directory -cne $p.service){throw 'Sealed combined policy and completed new capture required'}
if(Test-Path $OutputDirectory){throw 'New private output required'}
New-Item -ItemType Directory "$OutputDirectory\clone"|Out-Null
$pins=[ordered]@{};foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){$rows=@($m.files|Where-Object{$_.source_path -ceq "Windows/System32/config/$n"});if($rows.Count -ne 1 -or (Hash "$SourceDirectory\$n") -cne $rows[0].sha256 -or (Get-Item "$SourceDirectory\$n").Length -ne $rows[0].bytes){throw 'Original trio mismatch'};$pins[$n]=$rows[0].sha256;Copy-Item "$SourceDirectory\$n" "$OutputDirectory\clone\$n"}
Add-Type -Path "$PSScriptRoot\OfflineSystemHiveNative.cs"
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\prepare-offline-system-service.ps1",[ref]$t,[ref]$e);if($e.Count){throw 'Production snapshot parse failed'}
$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-ServiceSnapshot'},$true));if($f.Count -ne 1){throw 'One bounded production snapshot reader required'};Invoke-Expression $f[0].Extent.Text
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
$r=[ordered]@{schema=1;scope='read-only private replay; actual combined snapshots and fixed selections; not functional success';source_manifest_sha256=$ExpectedManifestSha256;policy_sha256=$ExpectedPolicySha256;source_files=$pins;nodes=[ordered]@{};old_services=[ordered]@{};service_preservation_comparisons=[ordered]@{};keypad_wdf_runtime=$null;full_service_preservation_equal=$true;combined=[ordered]@{};errors=@();load_rc=-1;unload_rc=-1;privilege_restore_error=-1;originals_unchanged=$false}
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
try{
 $priv=New-Object MT8167OfflineHive.Privileges;$r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\clone\SYSTEM");if($r.load_rc){throw 'Private replay load failed'};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 . (Join-Path $PSScriptRoot 'read-post-ownership-policy360.ps1')
 $r.post_ownership_policy360=Read-PostOwnershipPolicy360 $root
 if(!$r.post_ownership_policy360.standard_auto_acpi_post_enabled){throw 'Exact current policy360 DWORD0 required'}
 $select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false);try{$current=$select.GetValue('Current');if($current -ne 1){throw 'Explicit ControlSet001 required'}}finally{$select.Dispose()};$cs='ControlSet001'
 foreach($name in $p.service_preservation.PSObject.Properties.Name){
  $v=Peripheral-ServiceSnapshot $root "$cs\Services\$name";$r.old_services[$name]=$v;$old=$p.service_preservation.$name
  $same=$v.present -eq $old.present -and $v.root_sha256 -ceq $old.root_sha256 -and $v.subtree_sha256 -ceq $old.subtree_sha256
  $r.service_preservation_comparisons[$name]=[ordered]@{equal=$same;expected=$old;observed=$v}
  if(!$same){$r.full_service_preservation_equal=$false;$r.errors+=@('Old service fingerprint changed: '+$name)}
 }
 $r.keypad_wdf_runtime=Keypad-WdfRuntimeObservation $root
 $vf=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-CapturedValues'},$true));if($vf.Count -ne 1){throw 'One fixed named-value reader required'};Invoke-Expression $vf[0].Extent.Text
 $r.captured_named_values=Peripheral-CapturedValues $root 1 $p.captured_registry_values
 $r.named_values_are_not_full_service_hashes=$true
 $failed=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167CombinedFollowup148",$false);if($failed){$failed.Dispose();throw 'Rejected149 service unexpectedly present'};$r.rejected149_service_absent=$true

 foreach($id in @('ACPI\BEN0001','MT8167\AFE','ACPI\MOU0001','MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007','ACPI\MOU0006','ACPI\MOU0010','ACPI\MOU0013','MT6392\RTC','MT8167\Thermal','MT6392\Buttons','ACPI\MOU0008')){
  $k=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\$id",$false);if(!$k){$r.nodes[$id]=@{present=$false};continue}
  try{$rows=@();$names=@($k.GetSubKeyNames());if($names.Count -gt 8){throw 'Fixed node instance bound exceeded'};foreach($n in $names){$leaf=[MT8167OfflineHive.Native]::OpenPath($root,("$cs\Enum\$id\"+$n),$false);try{$v=Values $leaf @('Service','ClassGUID','Driver','HardwareID','ConfigFlags','LowerFilters','UpperFilters');$row=[ordered]@{instance=$n;values=$v;selected_present=$false};if($v.Driver.present){$d=[string]$v.Driver.value;if($d -cnotmatch '^\{[a-fA-F0-9-]{36}\}\\[0-9]{4}$'){throw 'Unknown selected class path'};$dk=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Control\Class\$d",$false);if($dk){try{$row.selected=Values $dk @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName','LowerFilters','UpperFilters');$row.selected_present=$true}finally{$dk.Dispose()}}};$rows+=@($row)}finally{$leaf.Dispose()}};$r.nodes[$id]=@{present=$true;instances=$rows}}finally{$k.Dispose()}
 }
 $sg=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167RenderSingleGpu",$false);if(!$sg){throw 'Retained GPU service missing'};try{$r.old_gpu_service=Values $sg @('Start','ImagePath');if($sg.GetValue('Start') -ne 4){$r.errors+=@('Old GPU service no longer Start4')}}finally{$sg.Dispose()}
 $svc=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167CombinedFollowup200",$false);if(!$svc){$r.new_service=@{present=$false}}else{try{$r.new_service=@{present=$true;values=(Values $svc @('Start','ImagePath','Type','ObjectName','DependOnService'))}}finally{$svc.Dispose()}}
 foreach($phase in @('Install','Peripheral','Final','Diagnostic')){
  $k=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167CombinedFollowup200\Parameters\CombinedResult\$phase",$false);if(!$k){$r.combined[$phase]=@{present=$false};continue}
  try{$known=@('Schema','Complete','PolicyHash','RunId','ServicePid','ObservedResult','NodeCount','ClientCount','Nodes','Clients','SystemVolumeFlush');if($k.GetSubKeyNames().Count -or @($k.GetValueNames()|Where-Object{$_ -cnotin $known}).Count){throw 'Unknown combined leaf schema'};$v=Values $k $known;foreach($n in @('Schema','Complete','RunId','ServicePid','ObservedResult','NodeCount','ClientCount')){if($v[$n].present -and $v[$n].kind -cne 'DWord'){throw 'Unknown combined scalar type'}};foreach($n in @('Nodes','Clients','SystemVolumeFlush')){if($v[$n].present -and $v[$n].kind -cne 'Binary'){throw 'Unknown combined binary type'}};if($v.Complete.present -and $v.Complete.value -notin @(0,1)){throw 'Unknown Complete value'};$r.combined[$phase]=@{present=$true;values=$v};if($v.PolicyHash.present -and ($v.PolicyHash.kind -cne 'String' -or $v.PolicyHash.value -cne $ExpectedPolicySha256)){throw 'Combined policy identity mismatch'};if($v.Schema.present -and ($v.Schema.kind -cne 'DWord' -or $v.Schema.value -ne 2)){throw 'Unknown combined schema'};if($v.Complete.present -and $v.Complete.value -eq 1){if(!$v.PolicyHash.present -or $v.NodeCount.value -ne 13 -or $v.ClientCount.value -ne $readerClientCountSchema -or $v.Nodes.kind -cne 'Binary' -or $v.Nodes.value.Length -ne 832 -or $v.Clients.kind -cne 'Binary' -or $v.Clients.value.Length -ne 576){throw 'Completed observation snapshot shape mismatch'}}}finally{$k.Dispose()}
 }
 $k=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\GE8300Render\Parameters\FactoryCheckpoint",$false);if(!$k){$r.gpu_factory=@{present=$false}}else{try{$n=@($k.GetValueNames()|Where-Object{$_ -cmatch '^R[0-6][0-9]$'});$r.gpu_factory=@{present=$true;values=(Values $k $n 90)}}finally{$k.Dispose()}}

 # Additive fixed init fields, interpreted only with ReachedMask and same-capture identity.
 . "$PSScriptRoot\read-gpu-native-checkpoint.ps1"
 . "$PSScriptRoot\read-mt6630-start-checkpoint.ps1"
 $btroot=[Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($mount,$false)
 try{$r.bt_start=Read-Mt6630StartCheckpoint -Root $btroot}finally{if($btroot){$btroot.Dispose()}}
 $r.gpu_init=Read-GpuNativeCheckpoint $root $cs

 $r.factory_progress_contract=$p.factory_progress_collection
 . (Join-Path $PSScriptRoot 'read-ge8300-boundaries352.ps1')
 $r.texture352_boundaries=ConvertFrom-Ge8300Boundaries352 -Values $r.gpu_factory.values
 . (Join-Path $PSScriptRoot 'read-ge8300-boundaries353.ps1')
 $r.irq_service353_boundaries=ConvertFrom-Ge8300Boundaries353 -Values $r.gpu_factory.values
 $r.irq_service353_boundaries.decoder_contract_buildid_is_reference_only=$true
 $identity=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\GE8300Render\Parameters\NativeCheckpoint",$false)
 if(!$identity){$r.module_identity=@{present=$false}}else{try{if('ModuleIdentityV1' -cin $identity.GetValueNames()){$hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($identity,'ModuleIdentityV1',3,144);$bytes=New-Object byte[] 144;for($i=0;$i -lt 144;$i++){$bytes[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)};[IO.File]::WriteAllBytes("$OutputDirectory\module-identity.bin",$bytes);$source=[Text.Encoding]::ASCII.GetString($bytes,48,48).TrimEnd([char]0);$build=[Text.Encoding]::ASCII.GetString($bytes,96,48).TrimEnd([char]0);$matches=$source -ceq $p.module_identity_collection.expected_sourcebase -and $build -ceq $p.module_identity_collection.expected_buildid
$r.module_identity=@{present=$true;bytes=144;sha256=(Hash "$OutputDirectory\module-identity.bin");observed_sourcebase=$source;observed_buildid=$build;expected_sourcebase=$p.module_identity_collection.expected_sourcebase;expected_buildid=$p.module_identity_collection.expected_buildid;baseline_buildid_reference_only=$p.baseline200_module_identity_reference_only.observed_buildid;expected_identity_matches=$matches;same_boot_correlation_required=$true}
if(!$matches){$r.errors+=@('Post198 module identity mismatch; baseline359 identity alone is not sameboot execution proof')}}else{$r.module_identity=@{present=$false}}}finally{$identity.Dispose()}}
 foreach($key in @('ROOT\MT6630COMBO','A04BR3\MT6630HCI')){
  $k=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\$key",$false);$row=@{present=$false;instances=@()};if($k){try{$row.present=$true;$names=@($k.GetSubKeyNames());if($names.Count -gt 8){throw 'BT instance bound exceeded'};foreach($name in $names){$leaf=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\$key\$name",$false);try{$row.instances+=@(@{instance=$name;values=(Values $leaf @('Service','ClassGUID','Driver','HardwareID','CompatibleIDs','ConfigFlags'))})}finally{$leaf.Dispose()}}}finally{$k.Dispose()}};$r.nodes[$key]=$row
 }
 $r.startup_predicates357=Read-Ge8300Start357 $root
 . (Join-Path $PSScriptRoot 'read-ge8300-start-gap359.ps1')
 $r.startup_gap359=Read-Ge8300StartGap359 $root
 $r.unified_display_bindings=[ordered]@{}
 foreach($binding in @(@{id='ACPI\MOU0007';service='MT8167DisplayTransport';class='{4d36e97d-e325-11ce-bfc1-08002be10318}';version='0.0.3.57'},@{id='ACPI\MOU0006';service='GE8300Render';class='{4d36e968-e325-11ce-bfc1-08002be10318}';version='0.0.3.68'})){
  $row=$r.nodes[$binding.id];$same=$row.present -and @($row.instances).Count -eq 1
  if($same){$v=$row.instances[0];$same=$v.instance -ceq '0' -and $v.values.Service.value -ieq $binding.service -and $v.values.ClassGUID.value -ieq $binding.class -and $v.selected_present -and $v.selected.DriverVersion.value -ceq $binding.version}
  $r.unified_display_bindings[$binding.id]=@{matches=$same;expected=$binding;observation=$row}
  if(!$same){$r.errors+=@('Post198 unified binding mismatch: '+$binding.id+'; installboot baseline is not diagnostic proof')}
 }
 $matches=@();$rk=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\ROOT",$false)
 if($rk){try{$devices=@($rk.GetSubKeyNames());if($devices.Count -gt 256){throw 'ROOT device bound exceeded'};foreach($device in $devices){$dk=[MT8167OfflineHive.Native]::OpenPath($root,("$cs\Enum\ROOT\"+$device),$false);try{$instances=@($dk.GetSubKeyNames());if($instances.Count -gt 32){throw 'ROOT instance bound exceeded'};foreach($instance in $instances){$path="$cs\Enum\ROOT\"+$device+'\'+$instance;$leaf=[MT8167OfflineHive.Native]::OpenPath($root,$path,$false);try{$hw=$leaf.GetValue('HardwareID');if($hw -and @($hw|Where-Object{$_ -ieq 'ROOT\MT6630COMBO'}).Count){$matches+=@(@{instance=($device+'\'+$instance);registry_path=$path;values=(Values $leaf @('Service','ClassGUID','Driver','HardwareID','CompatibleIDs','ConfigFlags'))})}}finally{$leaf.Dispose()}}}finally{$dk.Dispose()}}}finally{$rk.Dispose()}}
 if($matches.Count -gt 1){throw 'Duplicate BT root hardware identity'}
 $r.nodes['ROOT\MT6630COMBO']=@{present=($matches.Count -eq 1);instances=$matches;selection='exact hardware-ID match including generated ROOT/System instances'}

}catch{$r.errors+=@($_.Exception.Message)}finally{if($root){$root.Dispose()};if($loaded){$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)};if($priv){$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}}
$r.originals_unchanged=$true;foreach($n in $pins.Keys){if((Hash "$SourceDirectory\$n") -cne $pins[$n]){$r.originals_unchanged=$false;$r.errors+=@('Original trio changed')}}
$r|ConvertTo-Json -Depth 24|Set-Content -Encoding UTF8 "$OutputDirectory\receipt.json"
if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.originals_unchanged){throw 'Combined post receipt has explicit errors; keep JSON and clone'}
'COMBINED_POST_OBSERVATIONS_EXPORTED_NOT_A_FUNCTIONAL_SUCCESS'
