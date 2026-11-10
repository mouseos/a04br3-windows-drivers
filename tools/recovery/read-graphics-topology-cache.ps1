# Bounded saved display configuration only, on the caller's already loaded root.
# This does not query live CCD, identify an adapter by a cache name, or load a hive.
function Read-GraphicsTopologyCache($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $allowed=@('PrimSurfSize.cx','PrimSurfSize.cy','ActiveSize.cx','ActiveSize.cy','Position.cx','Position.cy',
  'Rotation','Scaling','ScanlineOrdering','PixelFormat','Stride','SourceId','TargetId','PathActive','Flags',
  'AdapterLuid.LowPart','AdapterLuid.HighPart','RefreshRate.Numerator','RefreshRate.Denominator',
  'RefreshRateDmd.Numerator','RefreshRateDmd.Denominator','VSyncFreq.Numerator','VSyncFreq.Denominator')
 $base="$ControlSet\Control\GraphicsDrivers"
 $result=[ordered]@{schema='GRAPHICS_TOPOLOGY_CACHE_DWORD_V1';source='saved private SYSTEM clone';
  scope='GraphicsDrivers/Configuration and Connectivity only';max_depth=8;max_keys=128;max_value_names_per_key=128;
  allowed_DWORD_names=$allowed;keys_visited=0;truncated=$false;errors=@();records=@();
  live_CCD_queried=$false;active_topology_verified=$false;adapter_owner_verified=$false;
  cached_entries_may_be_stale=$true;same_boot_correlation_required=$true;hardware_success_inferred=$false}
 # Breadth-first traversal starts with BOTH roots. Pending keys count against the cap.
 $queue=New-Object 'System.Collections.Generic.List[object]'
 foreach($name in @('Configuration','Connectivity')){$queue.Add([ordered]@{relative=$name;depth=0})}
 for($at=0;$at -lt $queue.Count;$at++){
  $item=$queue[$at];$key=$null
  $row=[ordered]@{relative_path=$item.relative;depth=$item.depth;present=$false;DWORD_values=[ordered]@{};
   ignored_non_DWORD=@();values_skipped=$false;children_skipped=0;limit_reason=$null}
  $result.keys_visited++
  try {
   $key=[MT8167OfflineHive.Native]::OpenPath($Root,($base+'\'+$item.relative),$false)
   if($key){
    $row.present=$true
    # Check metadata BEFORE enumerating names, so a large key cannot defeat the bound.
    if($key.ValueCount -gt 128){$row.values_skipped=$true;$result.truncated=$true}
    else{
     $names=@($key.GetValueNames())
     if($names.Count -gt 128){throw 'Display value-name bound changed during read'}
     foreach($name in $allowed){
      if($name -notin $names){continue}
      $kind=$key.GetValueKind($name)
      if($kind -ne [Microsoft.Win32.RegistryValueKind]::DWord){
       $row.ignored_non_DWORD+=@([ordered]@{name=$name;kind=[string]$kind});continue
      }
      $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($key,$name,4,4)
      $bytes=New-Object byte[] 4
      for($i=0;$i -lt 4;$i++){$bytes[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)}
      $v=[ordered]@{kind='DWord';bytes=4;raw_hex=$hex;value_uint32=[BitConverter]::ToUInt32($bytes,0)}
      if($name -cin @('Position.cx','Position.cy')){$v.value_int32=[BitConverter]::ToInt32($bytes,0)}
      $row.DWORD_values[$name]=$v
     }
    }
    $count=$key.SubKeyCount
    if($count -gt 0){
     $remaining=128-$queue.Count
     if($item.depth -ge 8){$row.children_skipped=$count;$row.limit_reason='max-depth';$result.truncated=$true}
     elseif($count -gt $remaining){$row.children_skipped=$count;$row.limit_reason='max-keys';$result.truncated=$true}
     else{
      $children=@($key.GetSubKeyNames()|Sort-Object)
      if($children.Count -gt $remaining){throw 'Display child-name bound changed during read'}
      foreach($child in $children){
       if(!$child -or $child.Length -gt 255 -or $child.Contains('\') -or $child.Contains('/')){throw 'Invalid display cache child name'}
       $queue.Add([ordered]@{relative=($item.relative+'\'+$child);depth=($item.depth+1)})
      }
     }
    }
   }
  }catch{
   $result.errors+=@([ordered]@{relative_path=$item.relative;error_type=$_.Exception.GetType().FullName;
    hresult_hex=('{0:X8}' -f ($_.Exception.HResult -band 4294967295L))})
  }finally{if($null -ne $key){$key.Dispose()}}
  $result.records+=@($row)
 }
 $result.complete=(!$result.truncated -and !$result.errors.Count)
 $result.status=if($result.errors.Count){'failed'}elseif($result.truncated){'bounded-partial-cache-observation'}else{'bounded-cache-observation'}
 return $result
}
