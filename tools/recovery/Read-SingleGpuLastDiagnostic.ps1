# Dot-source after loading OfflineSystemHiveNative.cs. No hive load/write/device IO.
# Root is the caller's already-open private replay root, never a command/path broker.
function Read-Mt8167SingleGpuLastDiagnostic($Root,[string]$ControlSet,[string]$PolicyHash) {
 if($ControlSet -cnotmatch '^ControlSet[0-9]{3}$' -or $PolicyHash -cnotmatch '^[0-9a-f]{64}$'){throw 'Fixed receipt controlset/policy pin invalid'}
 $result=[ordered]@{}
 foreach($phase in @('Install','Diagnostic')){
  $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\MT8167RenderSingleGpu\Parameters\LastDiagnostic\$PolicyHash\$phase",$false)
  if(!$key){$result[$phase]=@{present=$false};continue}
  try {
   function Dword($name){
    if($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::DWord){throw 'Fixed receipt DWORD type mismatch'}
    return [uint32]([int64]$key.GetValue($name) -band 0xffffffffL)
   }
   $schema=Dword 'Schema';$count=Dword 'TargetCount';$clients=Dword 'ClientCount'
   if($schema -ne 2 -or $count -ne 1 -or $clients -ne 1){throw 'Fixed codec/render single-GPU one-client receipt required'}
   if($key.GetValue('PolicyHash') -cne $PolicyHash){throw 'Stale diagnostic generation'}
   $values=[ordered]@{}
   $names=@('Schema','Complete','RunId','Phase','TargetCount','ClientCount','PolicyHash','ServiceResult','FileLogError','UartError','SnapshotWriteError')
   if($schema -eq 2){$names+=@('SystemVolumeFlush','SystemVolumeFlushError','CompletionResult','PnpBatchResult','InstallFailureValid','InstallFailureTarget','InstallFailureStep','AllInstallStepsSucceeded','DiagnosticArmed','RebootPending','DeferredPendingStatus','DiagnosticAttemptedStatus','RebootAttemptedStatus')}
   for($i=0;$i -lt $count;$i++){$names+=@("Node${i}HardwareId","Node${i}Before","Node${i}After")}
   for($i=0;$i -lt $clients;$i++){$names+=@("Client$i","Client${i}Stdout","Client${i}Stderr","Client${i}StdoutInfo","Client${i}StderrInfo")}
   if($key.GetSubKeyNames().Count -or $key.GetValueNames().Count -gt 64 -or @($key.GetValueNames()|Where-Object{$_ -cnotin $names}).Count){throw 'Unknown/bounded receipt value'}
   $ids=@('ACPI\MOU0006')
   for($i=0;$i -lt $count;$i++){if($key.GetValue("Node${i}HardwareId") -cne $ids[$i]){throw 'Fixed node identity mismatch'}}
   foreach($name in $names){
    $value=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    if($null -eq $value){$values[$name]=@{available=$false};continue}
    $kind=[string]$key.GetValueKind($name)
    if($value -is [byte[]]){
     $expected=if($name -match '^Node[0](Before|After)$'){44}elseif($name -match '^Client[0]$'){28}elseif($name -match '^Client[0](Stdout|Stderr)Info$'){20}elseif($name -ceq 'SystemVolumeFlush'){28}else{-1}
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
