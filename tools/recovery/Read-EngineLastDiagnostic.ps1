# Dot-source after loading OfflineSystemHiveNative.cs. No hive load/write/device IO.
# Root is the caller's already-open private replay root, never a command/path broker.
function Read-Mt8167EngineLastDiagnostic($Root,[string]$ControlSet,[string]$PolicyHash) {
 if($ControlSet -cnotmatch '^ControlSet[0-9]{3}$' -or $PolicyHash -cnotmatch '^[0-9a-f]{64}$'){throw 'Fixed receipt controlset/policy pin invalid'}
 $result=[ordered]@{}
 foreach($phase in @('Install','Diagnostic')){
  $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\MT8167EngineProbe\Parameters\LastDiagnostic\$PolicyHash\$phase",$false)
  if(!$key){$result[$phase]=@{present=$false};continue}
  try {
   function Dword($name){
    if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::DWord){throw 'Fixed receipt DWORD type mismatch'}
    return [uint32]([int64]$key.GetValue($name) -band 0xffffffffL)
   }
   $schema=Dword 'Schema';$count=Dword 'TargetCount';$clients=Dword 'ClientCount'
   if($schema -notin @(1,2) -or $count -lt 5 -or $count -gt $(if($schema -eq 1){6}else{7}) -or $clients -notin @(2,3)){throw 'Bounded fixed diagnostic schema/count mismatch'}
   if($key.GetValue('PolicyHash') -cne $PolicyHash){throw 'Stale diagnostic generation'}
   $values=[ordered]@{}
   $names=@('Schema','Complete','RunId','Phase','TargetCount','ClientCount','PolicyHash','ServiceResult','FileLogError','UartError','SnapshotWriteError')
   if($schema -eq 2){$names+=@('SystemVolumeFlush','SystemVolumeFlushError','CompletionResult','PnpBatchResult','InstallFailureValid','InstallFailureTarget','InstallFailureStep','AllInstallStepsSucceeded','DiagnosticArmed','RebootPending','DeferredPendingStatus','DiagnosticAttemptedStatus','RebootAttemptedStatus')}
   for($i=0;$i -lt $count;$i++){$names+=@("Node${i}HardwareId","Node${i}Before","Node${i}After")}
   for($i=0;$i -lt $clients;$i++){$names+=@("Client$i","Client${i}Stdout","Client${i}Stderr","Client${i}StdoutInfo","Client${i}StderrInfo")}
   $ids=@('ACPI\BEN0001','MT8167\AFE','MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007')
   if($count -gt 5){
    $sixth=[string]$key.GetValue('Node5HardwareId')
    if($sixth -notin @('ACPI\MOU0006','ACPI\MOU0013') -or ($schema -eq 1 -and $sixth -cne 'ACPI\MOU0006')){throw 'Fixed optional node mismatch'}
    $ids+=$sixth
   }
   if($count -eq 7){if($ids[5] -cne 'ACPI\MOU0006') {throw 'GPU/SD order mismatch'};$ids+='ACPI\MOU0013'}
   for($i=0;$i -lt $count;$i++){if($key.GetValue("Node${i}HardwareId") -cne $ids[$i]){throw 'Fixed node identity mismatch'}}
   foreach($name in $names){
    $value=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    if($null -eq $value){$values[$name]=@{available=$false};continue}
    $kind=[string]$key.GetValueKind($name)
    if($value -is [byte[]]){
     $expected=if($name -match '^Node[0-6](Before|After)$'){44}elseif($name -match '^Client[0-2]$'){28}elseif($name -match '^Client[0-2](Stdout|Stderr)Info$'){20}elseif($name -ceq 'SystemVolumeFlush'){28}else{-1}
     if($value.Length -gt 65536 -or ($expected -ge 0 -and $value.Length -ne $expected)){throw 'Fixed binary receipt length mismatch'}
     $value=[BitConverter]::ToString($value).Replace('-','')
    }
    $values[$name]=@{available=$true;kind=$kind;value=$value}
   }
   $result[$phase]=@{present=$true;values=$values}
  } finally {$key.Dispose()}
 }
 return $result
}
