# Already-loaded private root only. No EDID, driver loading, link following or writes.
function Read-DisplayDeviceBindings($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $result=[ordered]@{schema='DISPLAY_DEVICE_BINDINGS_V1';max_keys=64;max_depth=4;max_value_bytes=4096;
  records=@();errors=@();truncated=$false;keys_visited=0;live_topology_verified=$false;adapter_owner_verified=$false}
 $queue=New-Object 'System.Collections.Generic.List[object]'
 foreach($path in @('Enum\DISPLAY','Control\Video')){$queue.Add([ordered]@{path=$path;depth=0})}
 for($at=0;$at -lt $queue.Count;$at++){
  $item=$queue[$at];$key=$null;$result.keys_visited++
  $row=[ordered]@{path=$item.path;depth=$item.depth;present=$false;values=[ordered]@{};ignored_types=@();
   children_skipped=0;values_skipped=$false;registry_link_not_followed=$false;limit_reason=$null}
  if($item.path.StartsWith('Enum\DISPLAY',[StringComparison]::Ordinal)){
   $strings=@('HardwareID','CompatibleIDs','Service','Driver','ClassGUID','DeviceDesc','FriendlyName')
   $dwords=@('ConfigFlags','Problem','StatusFlags')
  }else{
   $strings=@('InstalledDisplayDrivers','Device Description','MatchingDeviceId','InfPath','InfSection','DriverVersion','ProviderName','Service','VideoID','DriverDesc','DeviceInstanceID')
   $dwords=@('Attach.ToDesktop','DefaultSettings.XResolution','DefaultSettings.YResolution','DefaultSettings.BitsPerPel','DefaultSettings.VRefresh','DefaultSettings.Orientation','VgaCompatible','Flags')
  }
  try{
   $key=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\'+$item.path),$false)
   if($key){
    $row.present=$true
    if($key.ValueCount -gt 128){$row.values_skipped=$true;$result.truncated=$true}
    else{
     $names=@($key.GetValueNames());if($names.Count -gt 128){throw 'Display binding value bound changed'}
     foreach($name in @($strings)+@($dwords)){
      if($name -notin $names){continue}
      $kind=$key.GetValueKind($name);$numeric=$name -in $dwords
      if(($numeric -and $kind -ne [Microsoft.Win32.RegistryValueKind]::DWord) -or (!$numeric -and $kind -notin @([Microsoft.Win32.RegistryValueKind]::String,[Microsoft.Win32.RegistryValueKind]::ExpandString,[Microsoft.Win32.RegistryValueKind]::MultiString))){$row.ignored_types+=@{name=$name;kind=[string]$kind};continue}
      $max=if($numeric){4}else{4096};$v=[MT8167OfflineHive.Native]::ReadRawValue($key,$name,$max)
      if(!$v.Present){throw 'Display binding value disappeared'}
      $bytes=New-Object byte[] $v.Bytes
      for($i=0;$i -lt $v.Bytes;$i++){$bytes[$i]=[Convert]::ToByte($v.RawHex.Substring($i*2,2),16)}
      if($numeric){if($v.Type -ne 4 -or $v.Bytes -ne 4){throw 'Display DWORD changed'};$value=[BitConverter]::ToUInt32($bytes,0)}
      else{
       if($v.Type -notin @(1,2,7) -or $v.Bytes%2){throw 'Display string changed'}
       $text=[Text.Encoding]::Unicode.GetString($bytes).TrimEnd([char]0)
       if($v.Type -eq 7){$value=@($text.Split([char]0));if($value.Count -gt 32){throw 'Display string array bound'}}else{$value=$text}
      }
      $row.values[$name]=[ordered]@{type=$v.Type;bytes=$v.Bytes;raw_hex=$v.RawHex;value=$value;environment_expansion_performed=$false}
     }
    }
    $count=$key.SubKeyCount;$left=64-$queue.Count
    if($count -gt 0){
     if($item.depth -ge 4 -or $count -gt $left){$row.children_skipped=$count;$row.limit_reason='depth-or-key-bound';$result.truncated=$true}
     else{foreach($child in @($key.GetSubKeyNames()|Sort-Object)){if(!$child -or $child.Length -gt 255 -or $child.Contains('\') -or $child.Contains('/')){throw 'Invalid binding child'};if($queue.Count -ge 64){throw 'Binding child bound changed'};$queue.Add([ordered]@{path=($item.path+'\'+$child);depth=($item.depth+1)})}}
    }
   }
  }catch{
   # Control\Video often contains registry links. The shared helper forbids them;
   # never follow such links into the running host registry or relax that guard.
   if($_.Exception.ToString().Contains('Registry link in the modification path')){$row.registry_link_not_followed=$true;$result.truncated=$true}
   else{$result.errors+=@{path=$item.path;error_type=$_.Exception.GetType().FullName}}
  }finally{if($null -ne $key){$key.Dispose()}}
  $result.records+=@($row)
 }
 $result.complete=(!$result.errors.Count -and !$result.truncated)
 return $result
}
