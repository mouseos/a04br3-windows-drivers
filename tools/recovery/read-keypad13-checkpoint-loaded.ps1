# Existing loaded private SYSTEM clone only. No load/unload or registry writes.
function Read-Keypad13Checkpoint($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Fixed keypad checkpoint controlset required'}
 $path="$ControlSet\Services\Mt6392Keys\Parameters\NativeCheckpoint"
 $r=[ordered]@{schema='MT8167_KEYPAD13_CHECKPOINT_V1';path=$path;present=$false;complete=$false;values=[ordered]@{};records=@();errors=@();current_epoch=$null;hardware_success_claimed=$false}
 $k=[MT8167OfflineHive.Native]::OpenPath($Root,$path,$false)
 if(!$k){$r.complete=$true;return $r}
 try {
  $r.present=$true
  if($k.GetSubKeyNames().Count){$r.errors+=@('Unexpected checkpoint child key')}
  $names=@($k.GetValueNames());if($names.Count -gt 32){$r.errors+=@('Checkpoint value bound exceeded');return $r}
  foreach($name in $names){
   if($name -cne 'LoadEpochV1' -and $name -cnotmatch '^B(?:0[1-9]|[12][0-9]|3[01])$'){$r.errors+=@('Unexpected checkpoint value name');continue}
   $v=[MT8167OfflineHive.Native]::ReadRawValue($k,$name,80)
   $r.values[$name]=[ordered]@{present=$v.Present;type=[int]$v.Type;bytes=$v.Bytes;raw_hex=$v.RawHex}
  }
  $epoch=$r.values['LoadEpochV1'];$epochBytes=$null
  if($epoch -and $epoch.type -eq 3 -and $epoch.bytes -eq 16){
   $epochBytes=New-Object byte[] 16;for($i=0;$i -lt 16;$i++){$epochBytes[$i]=[Convert]::ToByte($epoch.raw_hex.Substring(2*$i,2),16)}
   if([BitConverter]::ToUInt32($epochBytes,0) -ne 0x31424b41 -or [BitConverter]::ToUInt32($epochBytes,4) -ne 1 -or [BitConverter]::ToUInt64($epochBytes,8) -eq 0){$r.errors+=@('Invalid load epoch');$epochBytes=$null}else{$r.current_epoch=[BitConverter]::ToUInt64($epochBytes,8).ToString()}
  }elseif($names.Count){$r.errors+=@('Load epoch absent or malformed')}
  foreach($name in @($r.values.Keys|Sort-Object)){
   if($name -ceq 'LoadEpochV1'){continue};$v=$r.values[$name]
   if($v.type -ne 3 -or $v.bytes -ne 80){$r.errors+=@("$name record shape mismatch");continue}
   $data=New-Object byte[] 80;for($i=0;$i -lt 80;$i++){$data[$i]=[Convert]::ToByte($v.raw_hex.Substring(2*$i,2),16)}
   $words=@();for($i=0;$i -lt 20;$i++){$words+=@([BitConverter]::ToUInt32($data,4*$i))}
   $stage=[int]$name.Substring(1);$kind=if($stage -eq 13){3}elseif($stage -in @(11,14,26,27)){2}elseif($stage -in @(1,3,5,6,7,10,25)){0}else{1}
   $valid=($words[0] -eq 0x31424b41 -and $words[1] -eq 1 -and $words[2] -eq 80 -and $words[3] -eq $stage -and $words[4] -eq $kind -and !(@($words[16..19]|Where-Object {$_}).Count) -and !($kind -eq 0 -and $words[5]) -and !($kind -eq 2 -and $words[5] -gt 1))
   # B25..27 snapshot: raw16/raw16/flags5/backend-int/domain-pack/domain-pack.
   if($stage -in @(25,26,27)){
    if($words[6] -gt 65535 -or $words[7] -gt 65535 -or ($words[8] -band 0xffffffe0) -or !($words[8] -band 16)){$valid=$false}
    # known/value are constrained to the actual domain masks (PMIC9/keypad6).
    if(($words[10] -band 0xfff6fcf6) -or ($words[11] -band 0xfff9fcf9)){$valid=$false}
    if(($words[8] -band 2) -and !($words[8] -band 1)){$valid=$false}
    if(($words[8] -band 8) -and !($words[8] -band 4)){$valid=$false}
    if($stage -eq 26 -and ($words[5] -ne 1 -or $words[10] -ne 265 -or !($words[8] -band 2))){$valid=$false}
    if($stage -eq 27 -and ($words[5] -ne 1 -or $words[11] -ne 262 -or !($words[8] -band 8))){$valid=$false}
   }
   if($stage -ge 28){
    # Copy NTSTATUS success + returned VOID completion call, never OS consumption.
    if($words[5] -ge 0x80000000L -or $words[6] -ne ($stage-27) -or $words[7] -ne 1 -or $words[8] -ne 2 -or $words[9] -ne 3 -or $words[10] -or $words[11]){$valid=$false}
   }
   $own=[BitConverter]::ToUInt64($data,48);$time=[BitConverter]::ToUInt64($data,56)
   if(!$own -or $time -lt $own -or ($epochBytes -and $own -gt [BitConverter]::ToUInt64($epochBytes,8))){$valid=$false}
   $binding=if(!$epochBytes){'unknown'}elseif($own -eq [BitConverter]::ToUInt64($epochBytes,8)){'current-load'}else{'stale-prior-load'}
   $r.records+=@([ordered]@{name=$name;stage=$stage;kind=$words[4];status_hex=('0x{0:x8}' -f $words[5]);aux=@($words[6..11]);load_epoch=$own.ToString();event_filetime=$time.ToString();epoch_binding=$binding;schema_valid=$valid})
   if(!$valid){$r.errors+=@("$name record schema mismatch")}
  }
  $r.records=@($r.records|Sort-Object {[UInt64]$_.event_filetime})
  $r.input_boundary_observation=[ordered]@{current_load_stages=@($r.records|Where-Object {$_.schema_valid -and $_.epoch_binding -eq 'current-load' -and $_.stage -ge 25}|ForEach-Object {$_.stage});missing_or_stale_is_unknown=$true;current_boot_proven=$false;OS_input_consumption_proven=$false}
  $r.complete=(!$r.errors.Count)
 }catch{$r.errors+=@($_.Exception.Message)}finally{$k.Dispose()}
 return $r
}
