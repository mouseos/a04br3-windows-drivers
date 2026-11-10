# Fixed read-only observation on the caller's already loaded private SYSTEM clone.
# Does not load a hive, execute a service, or infer cross-boot functional success.
function Read-GpuNativeCheckpoint($Root, [string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $path="$ControlSet\Services\GE8300Render\Parameters\NativeCheckpoint"
 $k=[MT8167OfflineHive.Native]::OpenPath($Root,$path,$false)
 if(!$k){return [ordered]@{present=$false;path=$path;same_boot_correlation_required=$true}}
 try {
  $stages=@('DriverEntry','Contract','DxgkInitialize','AddDevice','DeviceInformation','NativeOwnerCreate','ConfigureAperture','StartOwner','DrawOpsV1','DrawOpsV2','StartDevice')
  # 11 status fields + ReachedMask + DdiVersion + RegistryPath =14 fixed fields.
  $fixed=@($stages)+@('ReachedMask','DdiVersion','RegistryPath')
  $raw=Values $k $fixed
  foreach($n in @($stages)+@('ReachedMask','DdiVersion')){
   if($raw[$n].present -and $raw[$n].kind -cne 'DWord'){throw 'Native checkpoint scalar must be REG_DWORD'}
  }
  if($raw.RegistryPath.present -and $raw.RegistryPath.kind -cne 'String'){throw 'Native checkpoint RegistryPath must be REG_SZ'}
  $mask=$null
  if($raw.ReachedMask.present){$mask=([long]$raw.ReachedMask.value -band 4294967295L)}
  $qualified=[ordered]@{}
  for($i=0;$i -lt $stages.Count;$i++){
   $name=$stages[$i];$v=$raw[$name];$reached=$null;$hex=$null
   if($null -ne $mask){$reached=($mask -band (1L -shl $i)) -ne 0}
   if($v.present){$hex=('{0:X8}' -f ([long]$v.value -band 4294967295L))}
   $qualified[$name]=[ordered]@{present=$v.present;reached_in_recorded_mask=$reached;ntstatus_hex=$hex;
    eligible_for_same_boot_interpretation=($reached -eq $true -and $v.present);
    unreached_value_may_be_stale=($reached -ne $true)}
  }
  return [ordered]@{present=$true;path=$path;fixed_field_count=14;values=$raw;
   reached_mask=$mask;stage_observations=$qualified;same_boot_correlation_required=$true;
   boot_identity_must_be_bound_by_capture_manifest=$true;
   module_identity_alone_distinguishes_pure_relink=$false;
   functional_success_inferred=$false;display_lifecycle364=(Read-Ge8300DisplayLifecycle364 $Root $ControlSet)}
 }finally{$k.Dispose()}
}

function Read-Ge8300DisplayLifecycle364($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $result=[ordered]@{schema=1;record_bytes=80;same_boot_join_required=$true;hardware_success_inferred=$false;records=[ordered]@{}}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\GE8300Render\Parameters\DisplayLifecycle364",$false)
 $names=@('StopDeviceV1','PowerStateV1','DrainForDisplayV1')
 try {
  for($kind=1;$kind -le 3;$kind++){
   $name=$names[$kind-1];$r=[ordered]@{present=$false;absence_is_success=$false}
   if($key -and $name -cin $key.GetValueNames()){
    $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($key,$name,3,80);$b=New-Object byte[] 80
    for($i=0;$i -lt 80;$i++){$b[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)}
    if([BitConverter]::ToUInt32($b,0) -ne 1 -or [BitConverter]::ToUInt32($b,4) -ne 80 -or [BitConverter]::ToUInt32($b,8) -ne $kind -or [BitConverter]::ToUInt32($b,16) -gt 1){throw 'Unknown lifecycle364 schema/size/kind/completed'}
    $r=[ordered]@{present=$true;sequence=[BitConverter]::ToUInt32($b,12);completed=[BitConverter]::ToUInt32($b,16);status_hex=('{0:X8}' -f [BitConverter]::ToUInt32($b,20));argument0=[BitConverter]::ToUInt32($b,24);argument1=[BitConverter]::ToUInt32($b,28);before_state=[BitConverter]::ToUInt32($b,32);after_state=[BitConverter]::ToUInt32($b,36);before_started=[BitConverter]::ToUInt32($b,40);after_started=[BitConverter]::ToUInt32($b,44);begin_FILETIME=[BitConverter]::ToUInt64($b,48);begin_interrupttime=[BitConverter]::ToUInt64($b,56);end_FILETIME=[BitConverter]::ToUInt64($b,64);end_interrupttime=[BitConverter]::ToUInt64($b,72);last_record_only=$true;zero_is_return_only_not_hardware_success=$true}
   }
   $result.records[$name]=$r
  }
 }finally{if($key){$key.Dispose()}}
 return $result
}
