# Caller owns one already-loaded private SYSTEM clone. No load/write/replay here.
function Read-Lc709203fFirstFailure($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 only'}
 $path='ControlSet001\Services\LC709203F\Parameters\NativeCheckpoint'
 $r=[ordered]@{path=$path;present=$false;same_loaded_root=$true;hardware_success_claimed=$false;values=[ordered]@{};epoch_matches=$null;schema_valid=$null}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,$path,$false)
 if(!$key){return $r}
 try {
  $r.present=$true
  foreach($spec in @(@('LoadEpochV1',16),@('FirstFailureV1',96))){
   $v=[MT8167OfflineHive.Native]::ReadRawValue($key,$spec[0],[int]$spec[1])
   $r.values[$spec[0]]=[ordered]@{present=$v.Present;type=$v.Type;bytes=$v.Bytes;raw_hex=$v.RawHex}
  }
  $e=$r.values.LoadEpochV1;$f=$r.values.FirstFailureV1
  if($e.present -and $f.present){
   if($e.type -ne 3 -or $e.bytes -ne 16 -or $f.type -ne 3 -or $f.bytes -ne 96){$r.schema_valid=$false;return $r}
   $eb=New-Object byte[] 16;$fb=New-Object byte[] 96
   for($i=0;$i -lt 16;$i++){$eb[$i]=[Convert]::ToByte($e.raw_hex.Substring(2*$i,2),16)}
   for($i=0;$i -lt 96;$i++){$fb[$i]=[Convert]::ToByte($f.raw_hex.Substring(2*$i,2),16)}
   $r.schema_valid=([BitConverter]::ToUInt32($eb,0) -eq 0x3146474c -and [BitConverter]::ToUInt32($eb,4) -eq 1 -and [BitConverter]::ToUInt32($fb,0) -eq 0x3146474c -and [BitConverter]::ToUInt32($fb,4) -eq 1 -and [BitConverter]::ToUInt32($fb,8) -eq 96)
   if($r.schema_valid){$r.epoch_matches=([BitConverter]::ToUInt64($eb,8) -eq [BitConverter]::ToUInt64($fb,20))}
  }
 }finally{$key.Dispose()}
 return $r
}
