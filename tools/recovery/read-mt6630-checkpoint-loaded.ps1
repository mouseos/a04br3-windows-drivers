# Fixed leaves reused from selected-reader619438d, exercised in native208.
# Caller owns the single loaded private root and all cleanup/privilege handling.
# No RegLoadKey, service execution, WLAN profile or credential enumeration.
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
function Test-Mt6630CheckpointRecord([byte[]]$b,[uint32]$channel){
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

function Read-Mt6630LoadedCheckpoint($Root,[string]$ControlSet,[string]$OutputDirectory,[string]$SourceManifestSha256,[string]$RecoveryBootId) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 if($SourceManifestSha256 -cnotmatch '^[0-9a-f]{64}$'){throw 'Real capture manifest SHA required'}
 if(Test-Path -LiteralPath $OutputDirectory){throw 'New fixed checkpoint output required'}
 New-Item -ItemType Directory $OutputDirectory|Out-Null
 $r=[ordered]@{schema='MT6630_SAME_LOAD_CHECKPOINT_V1';source_manifest_sha256=$SourceManifestSha256;recovery_boot_id=$RecoveryBootId;
  registry_path='ControlSet001\Services\Mt6630Runtime\Parameters\NativeCheckpoint';values=[ordered]@{};record_headers=[ordered]@{};
  RuntimeFailure_first_only_per_adapter=$true;Scan_max_records_per_adapter=4;registry_write_requested=$false;
  same_boot_binding_verified=$false;connection_attempt_binding_verified=$false;connection_success_claimed=$false;cause_determined=$false;
  clone_replay_may_modify_clone=$true;checkpoint_tail_flush_proven=$false;errors=@()}
 $key=$null;$outputs=@()
 try {
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
   $gen=Test-Mt6630CheckpointRecord $b $channel
   if($name -ceq 'RuntimeFailure' -and [BitConverter]::ToUInt32($b,84) -eq 1029){$r.RuntimeFailure_first_only_per_adapter=$false}
   if(!$r.values.Generation.present -or $gen -gt $r.values.Generation.value){throw 'Record generation exceeds/misses saved counter'}
   $p=Join-Path $OutputDirectory ($name+'.bin');[IO.File]::WriteAllBytes($p,$b);$outputs+=@($p)
   $r.values[$name]=@{present=$true;type=3;bytes=1624;sha256=(Hash $p);generation=$gen;parent_generation=[BitConverter]::ToUInt64($b,24);interrupt_time=[BitConverter]::ToUInt64($b,32);system_time=[BitConverter]::ToUInt64($b,40);legacy_driver_build=[BitConverter]::ToUInt32($b,84);matches_1027_internal_build=([BitConverter]::ToUInt32($b,84) -eq 1023);connection_trace_build1029=([BitConverter]::ToUInt32($b,84) -eq 1029);attempt_binding='unverified'}
  }
 }else{foreach($name in @('Generation','DriverEntry','Initialize','Scan','RuntimeFailure')){$r.values[$name]=@{present=$false}}}

  $r.failed_path=$null
 }finally{if($key){$key.Dispose()}}
 return $r
}
