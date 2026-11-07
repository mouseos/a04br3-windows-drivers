# Dot-source with OfflineSystemHiveNative.cs already loaded. No load/write/device IO.
function Read-Mt6630RuntimeCheckpoint($Root,[string]$ControlSet){
 if($ControlSet -cnotmatch '^ControlSet[0-9]{3}$'){throw 'Fixed control set required'}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\Mt6630Runtime\Parameters\NativeCheckpoint",$false)
 if(!$key){return @{present=$false}}
 try{
  $result=[ordered]@{present=$true;generation=@{present=$false};records=[ordered]@{}}
  if(@($key.GetValueNames()) -contains 'Generation'){
   if($key.GetValueKind('Generation') -ne [Microsoft.Win32.RegistryValueKind]::QWord){throw 'Generation must be QWORD'}
   $result.generation=@{present=$true;value=$key.GetValue('Generation')}
  }
  $channels=[ordered]@{DriverEntry=0;Initialize=1;Scan=2;RuntimeFailure=3}
  foreach($name in $channels.Keys){
   if(@($key.GetValueNames()) -notcontains $name){$result.records[$name]=@{present=$false};continue}
   if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::Binary){throw 'Fixed checkpoint must be REG_BINARY'}
   $value=$key.GetValue($name)
   if($value.Length -ne 1624 -or [BitConverter]::ToUInt32($value,0) -ne 0x50433441 -or [BitConverter]::ToUInt32($value,4) -ne 1 -or [BitConverter]::ToUInt32($value,8) -ne 1624 -or [BitConverter]::ToUInt32($value,12) -ne $channels[$name]){throw 'Fixed checkpoint length/magic/schema/channel mismatch'}
   if([BitConverter]::ToUInt32($value,48) -gt 64 -or [BitConverter]::ToUInt32($value,64) -gt 10 -or [BitConverter]::ToUInt32($value,76) -ne 0){throw 'Fixed checkpoint bounds/tail claim mismatch'}
   $result.records[$name]=@{present=$true;kind='Binary';bytes=1624;channel=$channels[$name];driver_build=[BitConverter]::ToUInt32($value,84);interrupt_time=[BitConverter]::ToUInt64($value,32);system_time=[BitConverter]::ToUInt64($value,40);generation=[BitConverter]::ToUInt64($value,16);parent_generation=[BitConverter]::ToUInt64($value,24);hex=[BitConverter]::ToString($value).Replace('-','')}
  }
  return $result
 }finally{$key.Dispose()}
}
