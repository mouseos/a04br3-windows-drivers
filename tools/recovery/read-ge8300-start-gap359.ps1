# Fixed offline clone only. Zero without completed/return/identity/time is not success.
function Read-Ge8300StartGap359($Root) {
 $result=[ordered]@{schema=1;record_bytes=384;hardware_success_inferred=$false;rootfix_inferred=$false;records=[ordered]@{}}
 $specs=@(@{name='MiniportV1';kind=1},@{name='BridgeV1';kind=2},@{name='BddV1';kind=3},@{name='TransportQueryV1';kind=4})
 foreach($spec in $specs){
  $r=[ordered]@{present=$false;same_boot_join_required=$true;absence_is_success=$false}
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,'ControlSet001\Services\GE8300Render\Parameters\StartGap359',$false)
  if($k){try{if($spec.name -cin $k.GetValueNames()){
   $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,$spec.name,3,384);$b=New-Object byte[] 384
   for($i=0;$i -lt 384;$i++){$b[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)}
   if([BitConverter]::ToUInt32($b,0) -ne 1 -or [BitConverter]::ToUInt32($b,4) -ne 384 -or [BitConverter]::ToUInt32($b,8) -ne $spec.kind -or [BitConverter]::ToUInt32($b,12) -ne 359){throw 'Unknown fixed359 schema/kind/size/build'}
   $statuses=@();for($i=0;$i -lt 16;$i++){$statuses+=,('{0:X8}' -f [BitConverter]::ToUInt32($b,64+$i*4))}
   $values=@();for($i=0;$i -lt 64;$i++){$values+=,[BitConverter]::ToUInt32($b,128+$i*4)}
   $end=[BitConverter]::ToUInt64($b,56);$began=[BitConverter]::ToUInt64($b,40)
   $r=[ordered]@{present=$true;schema=1;bytes=384;kind=$spec.kind;build=359;entered_mask=[BitConverter]::ToUInt32($b,16);completed_mask=[BitConverter]::ToUInt32($b,20);last_step=[BitConverter]::ToUInt32($b,24);return_status_hex=('{0:X8}' -f [BitConverter]::ToUInt32($b,28));begin_FILETIME=[BitConverter]::ToUInt64($b,32);begin_interrupttime=$began;end_FILETIME=[BitConverter]::ToUInt64($b,48);end_interrupttime=$end;returned=($began -ne 0 -and $end -ge $began -and $end -ne 0 -and [BitConverter]::ToUInt64($b,32) -ne 0 -and [BitConverter]::ToUInt64($b,48) -ge [BitConverter]::ToUInt64($b,32));step_statuses=$statuses;values=$values;raw_binary_hex=$hex;same_boot_join_required=$true;zero_is_return_only_not_hardware_success=$true}
  }}finally{$k.Dispose()}}
  $result.records[$spec.name]=$r
 }
 return $result
}
