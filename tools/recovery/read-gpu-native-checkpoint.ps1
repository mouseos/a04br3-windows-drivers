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
   functional_success_inferred=$false;post_release_stack366=(Read-Ge8300PostStack366 $Root $ControlSet);retirement365=(Read-Ge8300Retirement365 $Root $ControlSet);display_lifecycle364=(Read-Ge8300DisplayLifecycle364 $Root $ControlSet)}
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

# One record per stage; FILETIME must join the captured boot and fresh R36.
# An absent record proves neither a missing callback nor successful hardware.
function Read-Ge8300Retirement365($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $result=[ordered]@{schema=2;record_bytes=40;same_boot_join_required=$true;last_record_only=$true;hardware_success_inferred=$false;records=[ordered]@{}}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\GE8300Render\Parameters\FactoryCheckpoint",$false)
 try {
  for($stage=80;$stage -le 89;$stage++){
   $name=('R{0:D2}' -f $stage);$r=[ordered]@{present=$false;absence_is_success=$false}
   if($key -and $name -cin $key.GetValueNames()){
    $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($key,$name,3,40);$b=New-Object byte[] 40
    for($i=0;$i -lt 40;$i++){$b[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16)}
    if([BitConverter]::ToUInt32($b,0) -ne 2 -or [BitConverter]::ToUInt32($b,4) -ne 40 -or [BitConverter]::ToUInt32($b,8) -ne $stage){throw 'Unknown retirement365 schema/size/stage'}
    $r=[ordered]@{present=$true;status_hex=('{0:X8}' -f [BitConverter]::ToUInt32($b,12));a=[BitConverter]::ToUInt32($b,16);b=[BitConverter]::ToUInt32($b,20);c=[BitConverter]::ToUInt32($b,24);d=[BitConverter]::ToUInt32($b,28);FILETIME=[BitConverter]::ToUInt64($b,32)}
   }
   $result.records[$name]=$r
  }
 }finally{if($key){$key.Dispose()}}
 return $result
}

function Read-Ge8300PostStack366($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $r=[ordered]@{present=$false;same_boot_join_required=$true;hardware_success_inferred=$false;missing_is_unknown=$true}
 $k=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\GE8300Render\Parameters\PostReleaseStack366",$false)
 try {
  if($k -and 'StackV1' -cin $k.GetValueNames()){
   $hex=[MT8167OfflineHive.Native]::ReadFixedRawValue($k,'StackV1',3,320);$b=New-Object byte[] 320
   for($i=0;$i -lt 320;$i++){$b[$i]=[Convert]::ToByte($hex.Substring(2*$i,2),16)}
   $count=[BitConverter]::ToUInt32($b,12);$mask=[BitConverter]::ToUInt32($b,32)
   if([BitConverter]::ToUInt32($b,0) -ne 1 -or [BitConverter]::ToUInt32($b,4) -ne 320 -or $count -gt 16 -or ($mask -band 4294836224L)){throw 'Unknown post stack366 schema/size/count/mask'}
   $frames=@()
   for($i=0;$i -le $count;$i++){
    $at=48+16*$i;$frames+=@([ordered]@{index=$i;direct_return=($i -eq 0);normalized=(($mask -band (1 -shl $i)) -ne 0);rva=[BitConverter]::ToUInt32($b,$at);image_bytes=[BitConverter]::ToUInt32($b,$at+4);timestamp=[BitConverter]::ToUInt32($b,$at+8);checksum=[BitConverter]::ToUInt32($b,$at+12)})
   }
   $r=[ordered]@{present=$true;schema=1;bytes=320;lookup_mask=[BitConverter]::ToUInt32($b,8);captured=$count;normalized_mask=$mask;FILETIME=[BitConverter]::ToUInt64($b,16);interrupt_time=[BitConverter]::ToUInt64($b,24);frames=$frames;last_record_only=$true;same_boot_join_required=$true;unwind_completeness_proven=$false;image_identity_must_match_captured_PE=$true;absolute_addresses_persisted=$false;hardware_success_inferred=$false}
  }
 }finally{if($k){$k.Dispose()}}
 return $r
}
