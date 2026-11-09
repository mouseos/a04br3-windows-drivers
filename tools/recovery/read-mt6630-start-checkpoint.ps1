# Dot-source from an existing read-only native-post clone. No load/create/open-HCI/registration.
function Read-Mt6630StartCheckpoint {
 param([Parameter(Mandatory=$true)][Microsoft.Win32.RegistryKey]$Root)
 if($Root.Name -cnotmatch '^HKEY_LOCAL_MACHINE\\MT8167OfflineSYSTEM_[0-9a-fA-F]{32}$'){throw 'Existing private replay clone root required'}
 $out=[ordered]@{schema=1;scope='fixed MT6630 software start receipts; no hardware request';root_present=$false;selected_present=$false;same_boot_binding_required=$true;functional_success_inferred=$false;errors=@()}
 function Fixed-Key($path){[MT8167OfflineHive.Native]::OpenPath($Root,$path,$false)}
 function Fixed-Dword($key,$name){
  if($name -cnotin $key.GetValueNames()){return @{present=$false}}
  if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::DWord){throw 'Unexpected status type'}
  $v=[long]$key.GetValue($name);return @{present=$true;value=$v;ntstatus_hex=('{0:X8}' -f ($v -band 0xffffffffL))}
 }
 $rootKey=Fixed-Key 'ControlSet001\Enum\ROOT\SYSTEM\0001'
 if(!$rootKey){return $out}
 try {
  $out.root_present=$true
  if($rootKey.GetValueNames().Count -gt 64 -or $rootKey.GetSubKeyNames().Count -gt 32){throw 'Root leaf bound exceeded'}
  $service=$rootKey.GetValue('Service');$driver=$rootKey.GetValue('Driver')
  if($service -cne 'Mt6630ComboBluetooth' -or $driver -notmatch '^\{4D36E97D-E325-11CE-BFC1-08002BE10318\}\\[0-9]{4}$'){throw 'Fixed MT6630 root selection required'}
  $out.service=$service;$out.selected_class_path=$driver
 }finally{$rootKey.Dispose()}
 $key=Fixed-Key ('ControlSet001\Control\Class\'+$driver)
 if(!$key){throw 'Selected class key absent'}
 try {
  $out.selected_present=$true;$out.selection=[ordered]@{}
  foreach($n in @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId')){
   if($n -cnotin $key.GetValueNames()){$out.selection[$n]=@{present=$false};continue}
   if($key.GetValueKind($n) -ne [Microsoft.Win32.RegistryValueKind]::String){throw 'Unexpected selected field type'}
   $v=[string]$key.GetValue($n);if($v.Length -gt 1024){throw 'Selected string bound exceeded'};$out.selection[$n]=@{present=$true;value=$v}
  }
 }finally{$key.Dispose()}
 $key=Fixed-Key ('ControlSet001\Control\Class\'+$driver+'\MT6630StartCheckpoint')
 $out.checkpoint=@{present=$false}
 if($key){try {
  if($key.GetValueNames().Count -gt 64 -or $key.GetSubKeyNames().Count){throw 'Checkpoint bound exceeded'}
  $cp=[ordered]@{present=$true;Schema=(Fixed-Dword $key 'Schema');ReachedMask=(Fixed-Dword $key 'ReachedMask');stages=[ordered]@{};receipt=@{present=$false}}
  $names=@('DeviceCreated','WaitLock','IoTarget','Timer','Queues','ChildCreated','SidebandQI','ControllerQI','StaticChild','D0Entry','BrokerTargetOpen','BrokerHciOpen','OptionalController','D0Return','D0Exit','Capabilities','BrokerReceipt')
  for($i=0;$i -lt $names.Count;$i++){
   $v=Fixed-Dword $key $names[$i];$v.reached_in_saved_mask=$cp.ReachedMask.present -and (($cp.ReachedMask.value -band (1 -shl $i)) -ne 0)
   $v.old_unreached_value_may_be_stale=!$v.reached_in_saved_mask;$cp.stages[$names[$i]]=$v
  }
  if('BrokerOpenV1' -cin $key.GetValueNames()){
   if($key.GetValueKind('BrokerOpenV1') -ne [Microsoft.Win32.RegistryValueKind]::Binary){throw 'Unexpected broker receipt type'}
   [byte[]]$blob=$key.GetValue('BrokerOpenV1');if($blob.Length -ne 64){throw 'Broker receipt must be64B'}
   $v=@();for($i=0;$i -lt 16;$i++){$v+=@([BitConverter]::ToUInt32($blob,4*$i))}
   if($v[0] -ne 1 -or $v[1] -ne 64 -or !$v[3] -or ($v[11] -band 5) -ne 5 -or ($v[11] -band 0xfffffff8L) -or ($v[4] -band 0xfffe0001L) -or ($v[5] -band (-bnot $v[4])) -or ($v[6] -band (-bnot $v[5])) -or $v[7] -gt 16){throw 'Malformed broker receipt'}
   $cp.receipt=[ordered]@{present=$true;schema=$v[0];bytes=$v[1];generation=$v[2];sequence=$v[3];entered_mask=$v[4];returned_mask=$v[5];succeeded_mask=$v[6];last_stage=$v[7];broker_ntstatus_hex=('{0:X8}' -f $v[8]);backend_status=[BitConverter]::ToInt32($blob,36);backend_valid=(($v[11] -band 2) -ne 0);last_opcode_hex=('{0:X4}' -f $v[10]);start_filetime=[BitConverter]::ToUInt64($blob,48);start_interrupttime=[BitConverter]::ToUInt64($blob,56);current_receipt_stage_reached=$cp.stages.BrokerReceipt.reached_in_saved_mask;same_boot_binding_required=$true;HCI_or_RF_success_inferred=$false}
  }
  $out.checkpoint=$cp
 }finally{$key.Dispose()}}
 $out.driver_parameters=[ordered]@{present=$false;old_values_require_same_boot_binding=$true}
 $key=Fixed-Key 'ControlSet001\Services\Mt6630ComboBluetooth\Parameters'
 if($key){try{if($key.GetValueNames().Count -gt 64){throw 'Driver parameter bound exceeded'};$out.driver_parameters.present=$true;foreach($n in @('DriverCreated','AddEntered','DeviceCreateStatus')){$out.driver_parameters[$n]=Fixed-Dword $key $n}}finally{$key.Dispose()}}
 return $out
}
