# Fixed offline-clone observations; no live registry path, pointer or dump input.
function Read-Ge8300Start357($Root) {
 $result=[ordered]@{schema=1;record_bytes=832;hardware_success_inferred=$false;rootfix_inferred=$false;extra_flushes=0;records=[ordered]@{}}
 foreach($spec in @(@{id='native_output';path='ControlSet001\Services\GE8300Render\Parameters\StartCheckpoint357';name='NativeOutputV1';kind=1},@{id='display_start';path='ControlSet001\Services\MT8167DisplayTransport\Parameters\StartCheckpoint357';name='DisplayStartV1';kind=2},@{id='display_D0';path='ControlSet001\Services\MT8167DisplayTransport\Parameters\StartCheckpoint357';name='PowerD0V1';kind=3})){
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,$spec.path,$false);$r=[ordered]@{present=$false;absence_means='not-received, unreached or optional-record skipped/lost; never success';same_boot_join_required=$true}
  if($k){try{if($spec.name -cin $k.GetValueNames()){
   $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,$spec.name,3,832);$b=New-Object byte[] 832;for($i=0;$i -lt 832;$i++){$b[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)}
   if([BitConverter]::ToUInt32($b,0) -ne 1 -or [BitConverter]::ToUInt32($b,4) -ne 832 -or [BitConverter]::ToUInt32($b,8) -ne $spec.kind -or [BitConverter]::ToUInt32($b,12) -ne 357){throw 'Unknown fixed357 schema/kind/size/build'}
   $w=@();for($i=0;$i -lt 192;$i++){$w+=,[BitConverter]::ToUInt32($b,64+$i*4)}
   $r=[ordered]@{present=$true;schema=1;bytes=832;kind=$spec.kind;build=357;first_failure_id=[BitConverter]::ToUInt32($b,16);status_hex=('{0:X8}' -f [BitConverter]::ToUInt32($b,20));valid_flags=[BitConverter]::ToUInt32($b,24);last_step=[BitConverter]::ToUInt32($b,28);begin_FILETIME=[BitConverter]::ToUInt64($b,32);begin_interrupttime=[BitConverter]::ToUInt64($b,40);end_FILETIME=[BitConverter]::ToUInt64($b,48);end_interrupttime=[BitConverter]::ToUInt64($b,56);words=$w;raw_binary_hex=$hex;same_boot_join_required=$true;status0_is_return_only_not_hardware_success=$true}
  }}finally{$k.Dispose()}}
  $result.records[$spec.id]=$r
 }
 return $result
}
