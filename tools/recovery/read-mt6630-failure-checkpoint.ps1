# Fixed provider1027 Wi-Fi checkpoint only. Fresh Windows process, private replay clone.
param([Parameter(Mandatory=$true)][string]$WorkspaceRoot,
 [Parameter(Mandatory=$true)][string]$Manifest,
 [Parameter(Mandatory=$true)][string]$ExpectedManifestSha256,
 [Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][ValidateSet('baseline-only','failed-session-snapshot','post-update-session-snapshot')][string]$EvidenceRole)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$w=[IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
foreach($item in @($Manifest,$SourceDirectory,$OutputDirectory)){
 if(![IO.Path]::GetFullPath($item).StartsWith($w+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Root-local input/output required'}
}
if($ExpectedManifestSha256 -cnotmatch '^[0-9a-f]{64}$' -or (Hash $Manifest) -cne $ExpectedManifestSha256){throw 'Actual manifest pin mismatch'}
$m=Get-Content -LiteralPath $Manifest -Raw|ConvertFrom-Json
if($m.status -cne 'captured-unmounted' -or $m.read_only -ne $true -or $m.unmounted -ne $true -or $null -eq $m.errors -or $m.errors.Count){throw 'Completed read-only capture required'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'New output required'}
if('MT8167OfflineHive.Native' -as [type]){throw 'Fresh Windows PowerShell process required'}
$helper=Join-Path $PSScriptRoot 'ReadonlyInventory200HiveNative.cs'
if((Hash $helper) -cne '6963b1f6c0ece21debb4935205da9784385f20e16cf53bb44b53e749d9127a6a'){throw 'Existing clone helper byte pin mismatch'}
$pins=[ordered]@{}
foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){
 $rows=@($m.files|Where-Object {$_.source_path -ceq "Windows/System32/config/$name"})
 $file=Join-Path $SourceDirectory $name
 if($rows.Count -ne 1 -or (Hash $file) -cne $rows[0].sha256 -or (Get-Item -LiteralPath $file).Length -ne $rows[0].bytes){throw 'Original SYSTEM trio pin mismatch'}
 $pins[$name]=[ordered]@{sha256=$rows[0].sha256;bytes=$rows[0].bytes}
}
New-Item -ItemType Directory "$OutputDirectory\clone","$OutputDirectory\compiler-temp"|Out-Null
$r=[ordered]@{schema='MT6630_1027_FAILURE_CHECKPOINT_RECEIPT_V1';status='started';evidence_role=$EvidenceRole;
 source_contract_commit='73427b486f3e621dd998774f38f396fbac54a61d';source_manifest_sha256=$ExpectedManifestSha256;
 recovery_boot_id=$m.recovery_boot_id;original_files=$pins;source_recipe_sha256=(Hash $PSCommandPath);helper_sha256=(Hash $helper);
 registry_path='ControlSet001\Services\Mt6630Runtime\Parameters\NativeCheckpoint';
 values=[ordered]@{};record_headers=[ordered]@{};validation_error_code=$null;load_rc=-1;unload_rc=-1;privilege_restore_error=-1;originals_unchanged=$false;
 manifest_unchanged=$false;registry_write_requested=$false;ACL_changed=$false;clone_replay_may_modify_clone=$true;
 same_boot_binding_verified=$false;connection_attempt_binding_verified=$false;cause_determined=$false;
 connection_success_claimed=$false;RuntimeFailure_first_only_per_adapter=$true;Scan_max_records_per_adapter=4;
 checkpoint_tail_flush_proven=$false;failed_path=$null;failure_stage=$null;error_type=$null;error_hresult=$null;errors=@()}
# Read only the fixed Wi-Fi bus leaf and its selected Net class key. No
# user/network profile/credential enumeration and no second hive load.
function Read-WifiSelection($Root) {
 $out=[ordered]@{node_present=$false;selected_present=$false;instance=$null;service=$null;class_path=$null;fields=[ordered]@{};loaded_image_verified=$false}
 $bus=[MT8167OfflineHive.Native]::OpenPath($Root,'ControlSet001\Enum\A04BR3\MT6630SDIO',$false)
 if(!$bus){return $out}
 try {$children=@($bus.GetSubKeyNames());if($children.Count -gt 8){throw 'WiFi instance bound exceeded'}}finally{$bus.Dispose()}
 foreach($id in $children){
  if($id -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Unexpected WiFi instance name'}
  $leaf=[MT8167OfflineHive.Native]::OpenPath($Root,('ControlSet001\Enum\A04BR3\MT6630SDIO\'+$id),$false)
  if(!$leaf){continue}
  try {
   if('Service' -cnotin $leaf.GetValueNames()){continue}
   if($leaf.GetValueKind('Service') -ne [Microsoft.Win32.RegistryValueKind]::String){throw 'Unexpected WiFi service type'}
   if([string]$leaf.GetValue('Service') -cne 'Mt6630Runtime'){continue}
   if($out.node_present){throw 'Ambiguous WiFi service instances'}
   $out.node_present=$true;$out.instance='A04BR3\MT6630SDIO\'+$id;$out.service='Mt6630Runtime'
   if('Driver' -cnotin $leaf.GetValueNames()){continue}
   if($leaf.GetValueKind('Driver') -ne [Microsoft.Win32.RegistryValueKind]::String){throw 'Unexpected WiFi class type'}
   $driver=[string]$leaf.GetValue('Driver')
   if($driver -notmatch '^\{4D36E972-E325-11CE-BFC1-08002BE10318\}\\[0-9]{4}$'){throw 'Fixed Net class selection required'}
   $out.class_path=$driver
  }finally{$leaf.Dispose()}
 }
 if(!$out.class_path){return $out}
 $selected=[MT8167OfflineHive.Native]::OpenPath($Root,('ControlSet001\Control\Class\'+$out.class_path),$false)
 if(!$selected){return $out}
 try {
  $out.selected_present=$true
  foreach($name in @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName')){
   if($name -cnotin $selected.GetValueNames()){$out.fields[$name]=@{present=$false};continue}
   if($selected.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::String){throw 'Unexpected WiFi selected field type'}
   $value=[string]$selected.GetValue($name);if($value.Length -gt 1024){throw 'Selected WiFi string bound exceeded'}
   $out.fields[$name]=@{present=$true;value=$value}
  }
 }finally{$selected.Dispose()}
 return $out
}
function SaveReceipt {$r|ConvertTo-Json -Depth 20|Set-Content -LiteralPath "$OutputDirectory\receipt.json" -Encoding UTF8}
# Validate the 1624B provider1027 schema before exporting any fixed binary.
function Validate-Record([byte[]]$b,[uint32]$channel){
 if($b.Length -ne 1624){throw 'Fixed binary length mismatch'}
 $u=@();for($i=0;$i -lt 4;$i++){$u+=@([BitConverter]::ToUInt32($b,4*$i))}
 if($u[0] -ne 0x50433441 -or $u[1] -ne 1 -or $u[2] -ne 1624 -or $u[3] -ne $channel){throw 'Checkpoint header/schema mismatch'}
 $gen=[BitConverter]::ToUInt64($b,16);$count=[BitConverter]::ToUInt32($b,48)
 if(!$gen -or $count -gt 64 -or [BitConverter]::ToUInt32($b,64) -gt 10 -or [BitConverter]::ToUInt32($b,76) -ne 0){throw 'Checkpoint bounds mismatch'}
 foreach($offset in @(52,56,68,80)){if([BitConverter]::ToUInt32($b,$offset) -gt 1){throw 'Checkpoint boolean mismatch'}}
 $stages=@(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,59,65,66,67,68,69,80,81,82,83,84,85,86,87,88,89,90,91,92,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127)
 for($i=0;$i -lt $count;$i++){
  $off=88+24*$i;$stage=[BitConverter]::ToUInt32($b,$off);$phase=[BitConverter]::ToUInt32($b,$off+4);$kind=[BitConverter]::ToUInt32($b,$off+8);$observed=[BitConverter]::ToUInt32($b,$off+12)
  if($stage -notin $stages -or $phase -notin @(1,2,3) -or $kind -gt 4 -or $observed -gt 1 -or ($phase -eq 1 -and $observed)){throw 'Checkpoint event schema mismatch'}
 }
 for($i=(88+24*$count);$i -lt 1624;$i++){if($b[$i]){throw 'Unused event storage must be zero'}}
 return $gen
}
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$key=$null;$priv=$null;$loaded=$false;$stage='copy-trio';$outputs=@()
SaveReceipt
try {
 foreach($name in $pins.Keys){Copy-Item -LiteralPath (Join-Path $SourceDirectory $name) -Destination "$OutputDirectory\clone\$name";if((Hash "$OutputDirectory\clone\$name") -cne $pins[$name].sha256){throw 'Clone initial byte mismatch'}}
 $stage='load-helper';$oldTemp=$env:TEMP;$oldTmp=$env:TMP
 try {$env:TEMP="$OutputDirectory\compiler-temp";$env:TMP=$env:TEMP;Add-Type -Path $helper}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
 $stage='privileges';$priv=New-Object MT8167OfflineHive.Privileges
 $stage='load-clone';$r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\clone\SYSTEM");if($r.load_rc){throw 'Private clone RegLoadKey failed'};$loaded=$true
 $stage='open-private-root';$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $stage='verify-active-controlset';$select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false)
 if(!$select){throw 'Select key absent'}
 try {if($select.GetValueKind('Current') -ne [Microsoft.Win32.RegistryValueKind]::DWord -or [int]$select.GetValue('Current') -ne 1){throw 'Only actual active ControlSet001 supported'};$r.active_controlset=1}finally{$select.Dispose()}
 $stage='read-wifi-selection';$r.selection=Read-WifiSelection $root
 $stage='open-fixed-checkpoint';$r.failed_path=$r.registry_path
 $key=[MT8167OfflineHive.Native]::OpenPath($root,$r.registry_path,$false)
 $r.checkpoint_present=!!$key
 if($key){
  $stage='read-Generation';$raw=[MT8167OfflineHive.Native]::ReadRawValue($key,'Generation',8)
  if($raw.Present){
   if($raw.Type -ne 11 -or $raw.Bytes -ne 8){throw 'Generation must be REG_QWORD8'}
   $b=New-Object byte[] 8;for($i=0;$i -lt 8;$i++){$b[$i]=[Convert]::ToByte($raw.RawHex.Substring(2*$i,2),16)}
   $r.values.Generation=@{present=$true;type=11;bytes=8;value=[BitConverter]::ToUInt64($b,0)}
  }else{$r.values.Generation=@{present=$false}}
  $names=@('DriverEntry','Initialize','Scan','RuntimeFailure')
  for($channel=0;$channel -lt 4;$channel++){
   $name=$names[$channel];$stage='read-'+$name;$raw=[MT8167OfflineHive.Native]::ReadRawValue($key,$name,1624)
   if(!$raw.Present){$r.values[$name]=@{present=$false};continue}
   if($raw.Type -ne 3 -or $raw.Bytes -ne 1624){throw 'Checkpoint must be REG_BINARY1624'}
   $b=New-Object byte[] 1624;for($i=0;$i -lt 1624;$i++){$b[$i]=[Convert]::ToByte($raw.RawHex.Substring(2*$i,2),16)}
   $r.record_headers[$name]=@{magic=[BitConverter]::ToUInt32($b,0);version=[BitConverter]::ToUInt32($b,4);bytes=[BitConverter]::ToUInt32($b,8);channel=[BitConverter]::ToUInt32($b,12);count=[BitConverter]::ToUInt32($b,48);legacy_driver_build=[BitConverter]::ToUInt32($b,84)}
   $gen=Validate-Record $b $channel
   if($name -ceq 'RuntimeFailure' -and [BitConverter]::ToUInt32($b,84) -eq 1029){$r.RuntimeFailure_first_only_per_adapter=$false}
   if(!$r.values.Generation.present -or $gen -gt $r.values.Generation.value){throw 'Record generation exceeds/misses saved counter'}
   $p=Join-Path $OutputDirectory ($name+'.bin');[IO.File]::WriteAllBytes($p,$b);$outputs+=@($p)
   $r.values[$name]=@{present=$true;type=3;bytes=1624;sha256=(Hash $p);generation=$gen;parent_generation=[BitConverter]::ToUInt64($b,24);interrupt_time=[BitConverter]::ToUInt64($b,32);system_time=[BitConverter]::ToUInt64($b,40);legacy_driver_build=[BitConverter]::ToUInt32($b,84);matches_1027_internal_build=([BitConverter]::ToUInt32($b,84) -eq 1023);connection_trace_build1029=([BitConverter]::ToUInt32($b,84) -eq 1029);attempt_binding='unverified'}
  }
 }else{foreach($name in @('Generation','DriverEntry','Initialize','Scan','RuntimeFailure')){$r.values[$name]=@{present=$false}}}
 $r.failed_path=$null;$r.status='fixed-checkpoint-read-await-cleanup'
}catch{
 $codes=@('Fixed binary length mismatch','Checkpoint header/schema mismatch','Checkpoint bounds mismatch','Checkpoint boolean mismatch','Checkpoint event schema mismatch','Unused event storage must be zero','Generation must be REG_QWORD8','Checkpoint must be REG_BINARY1624','Record generation exceeds/misses saved counter')
 $index=[Array]::IndexOf($codes,[string]$_.Exception.Message);if($index -ge 0){$r.validation_error_code=$index+1}
 $r.status='failed';$r.failure_stage=$stage;$r.error_type=$_.Exception.GetType().FullName;$r.error_hresult=('{0:X8}' -f ($_.Exception.HResult -band 0xffffffffL));$r.errors+=@('Fixed checkpoint read failed; see numeric error/stage')
}finally{
 if($key){$key.Dispose()};if($root){$root.Dispose()}
 if($loaded){try{$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)}catch{$r.errors+=@('Clone unload exception')}}
 if($priv){try{$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}catch{$r.errors+=@('Privilege restore exception')}}
 $r.originals_unchanged=$true
 foreach($name in $pins.Keys){try{if((Hash (Join-Path $SourceDirectory $name)) -cne $pins[$name].sha256){$r.originals_unchanged=$false}}catch{$r.originals_unchanged=$false}}
 $r.manifest_unchanged=(Hash $Manifest) -ceq $ExpectedManifestSha256
 if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.originals_unchanged -or !$r.manifest_unchanged){$r.status='failed'}else{$r.status='checkpoint-extracted-not-a-connection-diagnosis'}
 SaveReceipt
}
if($r.status -ceq 'failed'){throw 'Checkpoint extraction failed; preserve receipt/private clone'}
'MT6630_FIXED_CHECKPOINT_EXTRACTED_NO_CONNECTION_CLAIM'
