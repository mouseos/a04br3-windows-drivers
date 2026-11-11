# MT6392 Buttons: bounded HID traversal, selection by stored parent evidence only.
# Already-loaded private SYSTEM only. No RegLoadKey, device/SCM API or writes.
function Read-ButtonsDeviceBindings($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 only'}
 $r=[ordered]@{schema='MT8167_MT6392_BUTTON_BINDINGS_V2';same_loaded_root=$true;registry_write_requested=$false;live_started_status_verified=$false;physical_input_verified=$false;services=[ordered]@{};nodes=@();hid_candidates=@();class_selections=@();errors=@();truncated=$false;max_hid_device_names=512;max_hid_instances=1024;max_hid_collections=32;hid_search_status='unknown';hid_search_scope='stored-parent-links-only-not-live-enumeration';hid_instances_examined=0;hid_unresolved_instances=0;wdf_runtime_reference='receipt.keypad_wdf_runtime'}
 function ReadButtonLeaf([string]$Relative,[string[]]$Names){
  $out=[ordered]@{path=$Relative;present=$false;read_complete=$false;values=[ordered]@{}};$k=$null
  try{
   $k=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\'+$Relative),$false)
   if(!$k){return $out};$out.present=$true
   if($k.ValueCount -gt 128){throw 'Buttons fixed-leaf value bound exceeded'}
   foreach($name in $Names){
    $v=[MT8167OfflineHive.Native]::ReadRawValue($k,$name,4096)
    if(!$v.Present){$out.values[$name]=@{present=$false};continue}
    if($v.Type -notin @(1,2,4,7,11)){throw 'Buttons typed value mismatch'}
    $b=New-Object byte[] $v.Bytes
    for($i=0;$i -lt $v.Bytes;$i++){$b[$i]=[Convert]::ToByte($v.RawHex.Substring(2*$i,2),16)}
    if($v.Type -eq 4){if($v.Bytes -ne 4){throw 'Buttons DWORD size'};$value=[BitConverter]::ToUInt32($b,0)}
    elseif($v.Type -eq 11){if($v.Bytes -ne 8){throw 'Buttons QWORD size'};$value=[BitConverter]::ToUInt64($b,0)}
    else{if($v.Bytes%2){throw 'Buttons UTF16 size'};$text=[Text.Encoding]::Unicode.GetString($b).TrimEnd([char]0);if($v.Type -eq 7){$value=@($text.Split([char]0));if($value.Count -gt 32){throw 'Buttons string array bound'}}else{$value=$text}}
    $out.values[$name]=[ordered]@{present=$true;type=$v.Type;bytes=$v.Bytes;raw_hex=$v.RawHex;value=$value;environment_expanded=$false}
   }
   $out.read_complete=$true
  }catch{$r.errors+=@{path=$Relative;error_type=$_.Exception.GetType().FullName;message=$_.Exception.Message}}
  finally{if($null -ne $k){$k.Dispose()}}
  return $out
 }
 function ReadButtonClass($Row){
  $d=$Row.values.Driver
  if(!$d -or !$d.present){return}
  $path=[string]$d.value
  if($path -cnotmatch '^\{[a-fA-F0-9-]{36}\}\\[0-9]{4}$'){$r.errors+=@{path=$Row.path;message='Invalid class suffix'};return}
  $r.class_selections+=@(ReadButtonLeaf ('Control\Class\'+$path) @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName','LowerFilters','UpperFilters'))
 }
 foreach($name in @('Mt6392Keys','mshidkmdf','kbdhid','kbdclass','hidserv','HidBth')){
  $r.services[$name]=ReadButtonLeaf ('Services\'+$name) @('ImagePath','Start','Type','ErrorControl','Group','ObjectName','DependOnService')
 }
 $node='Enum\MT6392\Buttons';$k=$null;$buttonParents=@();$buttonsComplete=$false
 try{
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\'+$node),$false)
  if($k){$instances=@($k.GetSubKeyNames());if($instances.Count -gt 8){throw 'Buttons instance bound exceeded'}
   foreach($instance in $instances){
    if($instance -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Invalid button instance'}
    $row=ReadButtonLeaf ($node+'\'+$instance) @('Service','Driver','ClassGUID','HardwareID','CompatibleIDs','LowerFilters','UpperFilters','ParentIdPrefix','Parent','ContainerID','ConfigFlags','Problem','StatusFlags','Capabilities')
    $buttonParents+=@($row)
    $r.nodes+=@($row);ReadButtonClass $row
    $r.nodes+=@(ReadButtonLeaf ($node+'\'+$instance+'\Control') @('ActiveService'))
   }
   $buttonsComplete=$true
  }
 }catch{$r.errors+=@{path=$node;message=$_.Exception.Message}}
 finally{if($null -ne $k){$k.Dispose()}}
 # Container equality alone is never ancestry proof (shared containers can have
 # unrelated functions). No CM/SetupAPI, property mutation, hive load or link following.
 function ButtonParentId([string]$Value){
  # Accept a stored device-instance ID, optionally rooted at Enum, not arbitrary paths.
  if($Value.StartsWith('Enum\',[StringComparison]::OrdinalIgnoreCase)){$Value=$Value.Substring(5)}
  if($Value -notmatch '^[A-Za-z0-9&._-]{1,128}\\[A-Za-z0-9&._-]{1,128}\\[A-Za-z0-9&._-]{1,128}$'){return ''}
  return $Value
 }
 function MatchButtonParent($Row,[string]$Instance){
  $proof=@();$explicit=''
  if($Row.values.Parent.present){
   if($Row.values.Parent.type -eq 1){$explicit=ButtonParentId ([string]$Row.values.Parent.value)}
   if(!$explicit){return @{status='unknown';reason='invalid-or-uninterpreted-Parent'}}
  }
  foreach($parent in $buttonParents){
   if(!$parent.read_complete){continue}
   $parentId=$parent.path.Substring(5)
   $direct=($explicit -and $explicit.Equals($parentId,[StringComparison]::OrdinalIgnoreCase))
   $prefix=$parent.values.ParentIdPrefix
   $prefixMatch=$false
   if($prefix.present -and $prefix.type -eq 1 -and [string]$prefix.value -match '^[A-Za-z0-9&._-]{1,128}$'){
    $prefixMatch=($Instance -match ('^'+[regex]::Escape([string]$prefix.value)+'&[0-9a-fA-F]{4}$'))
   }
   # A contradictory explicit Parent must not be overridden by a naming match.
   if($prefixMatch -and $explicit -and !$direct){return @{status='unknown';reason='conflicting-parent-evidence'}}
   if($direct -or $prefixMatch){
    $containerEqual=$false
    if($parent.values.ContainerID.present -and $Row.values.ContainerID.present){
     $containerEqual=([string]$parent.values.ContainerID.value).Equals([string]$Row.values.ContainerID.value,[StringComparison]::OrdinalIgnoreCase)
    }
    $proof+=@(@{parent_path=$parent.path;explicit_parent_equal=[bool]$direct;parent_prefix_equal=[bool]$prefixMatch;container_equal_support_only=$containerEqual})
   }
  }
  if($proof.Count -eq 1){return @{status='matched';evidence=$proof[0]}}
  if($proof.Count -gt 1){return @{status='unknown';reason='ambiguous-Buttons-parent'}}
  if($explicit){return @{status='other-parent'}}
  return @{status='unknown';reason='no-stored-causal-parent-link'}
 }
 $k=$null;$hidComplete=$false
 try{
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\Enum\HID'),$false)
  if($k){$devices=@($k.GetSubKeyNames()|Sort-Object);if($devices.Count -gt 512){throw 'HID device-name bound exceeded'}
   foreach($id in $devices){
    if($id -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Invalid HID family name'}
    $family=$null
    try{
     $family=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\Enum\HID\'+$id),$false)
     if(!$family){throw 'HID family disappeared'}
     $instances=@($family.GetSubKeyNames()|Sort-Object);if($instances.Count -gt 8){throw 'HID family instance bound'}
     foreach($instance in $instances){
      if($r.hid_instances_examined -ge 1024){throw 'HID traversal total instance bound'}
      $r.hid_instances_examined++
      if($instance -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Invalid HID instance'}
      # Probe relationship fields only; unrelated identity/service contents are not emitted.
      $path='Enum\HID\'+$id+'\'+$instance
      $probe=ReadButtonLeaf $path @('Parent','ContainerID')
      if(!$probe.present -or !$probe.read_complete){$r.hid_unresolved_instances++;continue}
      $match=MatchButtonParent $probe $instance
      if($match.status -eq 'unknown'){$r.hid_unresolved_instances++;continue}
      if($match.status -ne 'matched'){continue}
      if($r.hid_candidates.Count -ge 32){throw 'Related HID collection output bound'}
      $row=ReadButtonLeaf $path @('Service','Driver','ClassGUID','HardwareID','CompatibleIDs','ParentIdPrefix','Parent','ContainerID','ConfigFlags','Problem','StatusFlags','Capabilities')
      $row.parent_link_verified=$true
      $row.parent_evidence=$match.evidence
      $row.parent_link_scope='stored-registry-link-not-live-stack'
      $r.hid_candidates+=@($row);ReadButtonClass $row
     }
    }finally{if($null -ne $family){$family.Dispose()}}
   }
   $hidComplete=$true
  }
 }catch{$r.errors+=@{path='Enum\HID';message=$_.Exception.Message};$r.truncated=$true}
 finally{if($null -ne $k){$k.Dispose()}}
 $r.complete=($buttonsComplete -and $hidComplete -and !$r.errors.Count -and !$r.truncated)
 if($r.complete -and $buttonParents.Count -gt 0){
  if($r.hid_candidates.Count -gt 0){$r.hid_search_status='found'}
  elseif($r.hid_unresolved_instances -eq 0){$r.hid_search_status='not_found'}
 }
 # found means at least one proven stored relation; search may still be incomplete.
 $r.hid_search_complete=($r.complete -and $r.hid_unresolved_instances -eq 0)
 return $r
}
