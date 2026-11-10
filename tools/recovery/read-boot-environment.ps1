# Fixed read-only leaves on the caller's loaded private SYSTEM root.
# Saved flags alone do not establish WinPE, normal Windows, or current boot state.
function Read-BootEnvironment($Root,[string]$ControlSet) {
 if($ControlSet -cne 'ControlSet001'){throw 'Explicit ControlSet001 required'}
 $r=[ordered]@{schema='SAVED_SYSTEM_BOOT_ENVIRONMENT_V1';source='saved private SYSTEM clone';
  control_set=$ControlSet;MiniNT=[ordered]@{path="$ControlSet\Control\MiniNT";present=$false};
  Setup=[ordered]@{path='Setup';present=$false;values=[ordered]@{}};
  registry_write_requested=$false;live_environment_verified=$false;is_WinPE=$null;
  normal_Windows_verified=$false;same_boot_binding_required=$true}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Control\MiniNT",$false)
 try{$r.MiniNT.present=($null -ne $key)}finally{if($key){$key.Dispose()}}
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,'Setup',$false)
 try {
  $r.Setup.present=($null -ne $key)
  foreach($name in @('SystemSetupInProgress','SetupType','SetupPhase','OOBEInProgress')){
   $v=[ordered]@{present=$false;expected_REG_DWORD=$true}
   if($key){
    # Fixed-name lookup; never enumerate Setup values, paths or command lines.
    $kind=$null
    try{$kind=$key.GetValueKind($name)}catch [System.IO.IOException]{}
    if($null -ne $kind){
     $v.present=$true;$v.kind=[string]$kind
     if($kind -eq [Microsoft.Win32.RegistryValueKind]::DWord){
      $raw=[MT8167OfflineHive.Native]::ReadRawValue($key,$name,4)
      if(!$raw.Present -or $raw.Type -ne 4 -or $raw.Bytes -ne 4){throw 'Setup DWORD changed during read'}
      $b=New-Object byte[] 4;for($i=0;$i -lt 4;$i++){$b[$i]=[Convert]::ToByte($raw.RawHex.Substring(2*$i,2),16)}
      $v.raw_hex=$raw.RawHex;$v.value_uint32=[BitConverter]::ToUInt32($b,0)
     }else{$v.unexpected_type_not_decoded=$true}
    }
   }
   $r.Setup.values[$name]=$v
  }
 }finally{if($key){$key.Dispose()}}
 return $r
}
