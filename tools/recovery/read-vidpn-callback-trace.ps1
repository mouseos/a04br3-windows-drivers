# Dot-source and invoke INSIDE the caller's existing same-load reader.
# No hive load/unload, driver call, process launch or registry write.
function Read-VidPnCallbackTrace {
 param([Parameter(Mandatory=$true)][Microsoft.Win32.RegistryKey]$Root,
       [ValidatePattern('^ControlSet[0-9]{3}$')][string]$ControlSet='ControlSet001')
 $out=[ordered]@{schema='GE8300_VIDPN_CALLBACK_TRACE_V1';present=$false;epoch=$null;records=@();stale=@();errors=@();bounded_slots_per_kind=4;max_stored_values=44;latest_updated_each_safe_call=$true;PASSIVE_APC_skip_possible=$true;missing_is_not_success=$true;callback_absence_proven=$false;hardware_success=$false}
 $path=$ControlSet+'\Services\GE8300Render\Parameters\VidPnCallbackTraceV1'
 $key=$Root.OpenSubKey($path,$false);if(!$key){return $out}
 function Decode-VidPnRecord([byte[]]$b) {
  if($b.Length -ne 432 -or [BitConverter]::ToUInt32($b,0) -ne 1 -or [BitConverter]::ToUInt32($b,4) -ne 432){throw 'VidPn record schema/size'}
  $d=@();for($i=0;$i -lt 88;$i++){$d+=@([BitConverter]::ToUInt32($b,80+4*$i))}
  return [ordered]@{schema=1;bytes=432;kind=[BitConverter]::ToUInt32($b,8);slot=[BitConverter]::ToUInt32($b,12);completed=[BitConverter]::ToUInt32($b,16);status_hex=('{0:x8}' -f [BitConverter]::ToUInt32($b,20));sequence=[BitConverter]::ToUInt32($b,24);eligible=[BitConverter]::ToUInt32($b,28);epoch_FILETIME=[BitConverter]::ToUInt64($b,32);epoch_interrupttime=[BitConverter]::ToUInt64($b,40);begin_FILETIME=[BitConverter]::ToUInt64($b,48);begin_interrupttime=[BitConverter]::ToUInt64($b,56);end_FILETIME=[BitConverter]::ToUInt64($b,64);end_interrupttime=[BitConverter]::ToUInt64($b,72);words=$d;raw_hex=([BitConverter]::ToString($b).Replace('-','').ToLowerInvariant())}
 }
 try {
  $out.present=$true;$names=@($key.GetValueNames())
  if($names.Count -gt 44 -or $key.GetSubKeyNames().Count){throw 'VidPn bounded key shape'}
  if($names -cnotcontains 'EpochV1'){throw 'Missing current VidPn epoch; old records unknown'}
  if($key.GetValueKind('EpochV1') -ne [Microsoft.Win32.RegistryValueKind]::Binary){throw 'Epoch type'}
  $epoch=Decode-VidPnRecord ([byte[]]$key.GetValue('EpochV1'))
  if($epoch.kind -ne 0 -or $epoch.completed -gt 1 -or $epoch.eligible -ne 1 -or !$epoch.sequence -or !$epoch.epoch_interrupttime -or $epoch.words[0] -ne 7 -or $epoch.words[1] -ne 4 -or $epoch.words[2] -ne 0){throw 'Epoch fields'}
  $out.epoch=$epoch
  foreach($name in $names){
   if($name -ceq 'EpochV1'){continue}
   if($name -cnotmatch '^C([1-7])([SFLU])([0-3])$' -or ($Matches[2] -cne 'S' -and $Matches[3] -cne '0')){throw 'Unexpected VidPn label'}
   $kind=[uint32]$Matches[1];$failure=($Matches[2] -ceq 'F');$last=($Matches[2] -ceq 'L');$unsupported=($Matches[2] -ceq 'U');$slot=[uint32]$Matches[3]
   if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::Binary){throw 'Record type'}
   $rec=Decode-VidPnRecord ([byte[]]$key.GetValue($name))
   if($rec.kind -ne $kind -or $rec.completed -gt 1 -or $rec.eligible -ne 1 -or (!$failure -and !$last -and !$unsupported -and $rec.slot -ne $slot) -or ($unsupported -and ($kind -ne 1 -or $rec.completed -ne 1 -or $rec.status_hex -cne '00000000' -or $rec.words[31] -ne 0)) -or ($failure -and ($rec.completed -ne 1 -or [Convert]::ToUInt32($rec.status_hex,16) -lt 2147483648))){throw 'Record fields'}
   $rec.label=$name
   if($rec.sequence -ne $epoch.sequence -or $rec.epoch_FILETIME -ne $epoch.epoch_FILETIME -or $rec.epoch_interrupttime -ne $epoch.epoch_interrupttime){$out.stale+=@($name);continue}
   $out.records+=@($rec)
  }
  $again=Decode-VidPnRecord ([byte[]]$key.GetValue('EpochV1'))
  if($again.raw_hex -cne $epoch.raw_hex){throw 'Epoch changed while reading'}
 }catch{$out.errors+=@($_.Exception.Message)}finally{$key.Close()}
 return $out
}
