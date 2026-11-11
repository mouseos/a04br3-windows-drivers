# Fixed MT6392 Buttons and zero-VID/PID HID family from actual keypad1.2 source.
# Already-loaded private SYSTEM only. No RegLoadKey, device/SCM API or writes.
function Read-ButtonsDeviceBindings($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 only'}
 $r=[ordered]@{schema='MT8167_MT6392_BUTTON_BINDINGS_V1';same_loaded_root=$true;registry_write_requested=$false;live_started_status_verified=$false;physical_input_verified=$false;services=[ordered]@{};nodes=@();hid_candidates=@();class_selections=@();errors=@();truncated=$false;max_hid_device_names=512;max_hid_instances=32;wdf_runtime_reference='receipt.keypad_wdf_runtime'}
 function ReadButtonLeaf([string]$Relative,[string[]]$Names){
  $out=[ordered]@{path=$Relative;present=$false;values=[ordered]@{}};$k=$null
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
 $node='Enum\MT6392\Buttons';$k=$null;$buttonPrefixes=@()
 try{
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\'+$node),$false)
  if($k){$instances=@($k.GetSubKeyNames());if($instances.Count -gt 8){throw 'Buttons instance bound exceeded'}
   foreach($instance in $instances){
    if($instance -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Invalid button instance'}
    $row=ReadButtonLeaf ($node+'\'+$instance) @('Service','Driver','ClassGUID','HardwareID','CompatibleIDs','LowerFilters','UpperFilters','ParentIdPrefix','ConfigFlags','Problem','StatusFlags','Capabilities')
    if($row.values.ParentIdPrefix.present){$buttonPrefixes+=@([string]$row.values.ParentIdPrefix.value)}
    $r.nodes+=@($row);ReadButtonClass $row
    $r.nodes+=@(ReadButtonLeaf ($node+'\'+$instance+'\Control') @('ActiveService'))
   }
  }
 }catch{$r.errors+=@{path=$node;message=$_.Exception.Message}}
 finally{if($null -ne $k){$k.Dispose()}}
 # Enumerate names only; open only the actual driver's zero VID/PID collection family.
 # Candidate IDs are not asserted to be children of this PDO without parent evidence.
 $k=$null;$hidCount=0
 try{
  $k=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\Enum\HID'),$false)
  if($k){$devices=@($k.GetSubKeyNames());if($devices.Count -gt 512){throw 'HID device-name bound exceeded'}
   foreach($id in $devices){
    if($id -notmatch '^VID_0000&PID_0000(?:&REV_0001)?(?:&COL0[1-4])?$'){continue}
    $family=$null
    try{
     $family=[MT8167OfflineHive.Native]::OpenPath($Root,($ControlSet+'\Enum\HID\'+$id),$false)
     if(!$family){continue};$instances=@($family.GetSubKeyNames());if($instances.Count -gt 8){throw 'HID family instance bound'}
     foreach($instance in $instances){
      $hidCount++;if($hidCount -gt 32){throw 'HID candidate instance total bound'}
      if($instance -cnotmatch '^[A-Za-z0-9&._-]{1,128}$'){throw 'Invalid HID candidate instance'}
      $row=ReadButtonLeaf ('Enum\HID\'+$id+'\'+$instance) @('Service','Driver','ClassGUID','HardwareID','CompatibleIDs','ParentIdPrefix','ConfigFlags','Problem','StatusFlags','Capabilities')
      $parentMatches=@($buttonPrefixes|Where-Object {$instance.StartsWith($_+'&',[StringComparison]::OrdinalIgnoreCase)})
      $row.parent_prefix_match=($parentMatches.Count -gt 0);$row.parent_link_verified=$false;$row.selection_reason='Actual keypad1.2 HID attributes VID/PID0 family only; not live or definitive ancestry'
      $r.hid_candidates+=@($row);ReadButtonClass $row
     }
    }finally{if($null -ne $family){$family.Dispose()}}
   }
  }
 }catch{$r.errors+=@{path='Enum\HID';message=$_.Exception.Message};$r.truncated=$true}
 finally{if($null -ne $k){$k.Dispose()}}
 $r.complete=(!$r.errors.Count -and !$r.truncated)
 return $r
}
