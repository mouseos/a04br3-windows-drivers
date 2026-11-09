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
   functional_success_inferred=$false}
 }finally{$k.Dispose()}
}
