# Dot-source after loading OfflineSystemHiveNative.cs. No hive load/write/device IO.
# Root is the caller's already-open private replay root, never a command/path broker.
# No default exception: every incomplete generation needs an explicit allowlisted pin.
# This is an incomplete raw snapshot, never a verified node identity or success receipt.
function Assert-Mt8167SingleGpuPartialPolicies([string[]]$Policies,[string[]]$AllowedPolicies) {
 foreach($list in @(@{items=@($Policies)},@{items=@($AllowedPolicies)})){
  $items=$list.items
  if($items.Count -gt 16 -or @($items|Where-Object{$_ -cnotmatch '^[0-9a-f]{64}$'}).Count -or @($items|Select-Object -Unique).Count -ne $items.Count){throw 'Invalid or duplicate diagnostic policy pins'}
 }
 if(@($Policies|Where-Object{$_ -cnotin $AllowedPolicies}).Count){throw 'Partial diagnostic policies must be a subset of allowed diagnostic policies'}
}
function Read-Mt8167SingleGpuPartialHeader($Key,[string]$Phase,[string]$PolicyHash,[string[]]$PartialDiagnosticPolicies=@(),[string[]]$AllowedDiagnosticPolicies=@()) {
 Assert-Mt8167SingleGpuPartialPolicies $PartialDiagnosticPolicies $AllowedDiagnosticPolicies
 if($Phase -cne 'Diagnostic' -or $PolicyHash -cnotin $PartialDiagnosticPolicies){return $null}
 if($Key.GetValueKind('Complete') -ne [Microsoft.Win32.RegistryValueKind]::DWord){throw 'Partial diagnostic Complete type mismatch'}
 $complete=[int]$Key.GetValue('Complete')
 if($complete -eq 1){return $null}
 if($complete -ne 0){throw 'Partial diagnostic Complete must be 0 or 1'}
 $names=@('Schema','Complete','RunId','Phase','TargetCount','ClientCount','PolicyHash')
 $actual=@($Key.GetValueNames())
 if($Key.GetSubKeyNames().Count -or $actual.Count -ne 7 -or @($actual|Where-Object{$_ -cnotin $names}).Count){throw 'Partial diagnostic exact seven-value header required'}
 $values=[ordered]@{}
 foreach($name in $names){
  $type=if($name -ceq 'PolicyHash'){1}else{4};$bytes=if($type -eq 1){130}else{4}
  $raw=[MT8167OfflineHive.Native]::ReadFixedRawValue($Key,$name,$type,$bytes)
  $v=$Key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
  $values[$name]=@{available=$true;kind=[string]$Key.GetValueKind($name);value=$v;raw_hex=$raw;bytes=$bytes;type=$type}
 }
 foreach($pair in @(@('Schema',2),@('Complete',0),@('Phase',2),@('TargetCount',1),@('ClientCount',1))){
  if($values[$pair[0]].value -ne $pair[1]){throw 'Partial diagnostic fixed header mismatch'}
 }
 $expectedRaw=([BitConverter]::ToString([Text.Encoding]::Unicode.GetBytes($PolicyHash+[char]0))).Replace('-','').ToLowerInvariant()
 if($values.PolicyHash.value -cne $PolicyHash -or $values.PolicyHash.raw_hex -cne $expectedRaw){throw 'Partial diagnostic policy mismatch'}
 return @{present=$true;partial=$true;identity_verified=$false;identity_status='unverified';complete=$false;values=$values;raw_fingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($Key)}
}
function Read-Mt8167SingleGpuLastDiagnostic($Root,[string]$ControlSet,[string]$PolicyHash,[string[]]$PartialDiagnosticPolicies=@(),[string[]]$AllowedDiagnosticPolicies=@()) {
 if($ControlSet -cnotmatch '^ControlSet[0-9]{3}$' -or $PolicyHash -cnotmatch '^[0-9a-f]{64}$'){throw 'Fixed receipt controlset/policy pin invalid'}
 Assert-Mt8167SingleGpuPartialPolicies $PartialDiagnosticPolicies $AllowedDiagnosticPolicies
 $result=[ordered]@{}
 foreach($phase in @('Install','Diagnostic')){
  $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\MT8167RenderSingleGpu\Parameters\LastDiagnostic\$PolicyHash\$phase",$false)
  if(!$key){$result[$phase]=@{present=$false};continue}
  try {
   $partial=Read-Mt8167SingleGpuPartialHeader $key $phase $PolicyHash $PartialDiagnosticPolicies $AllowedDiagnosticPolicies
   if($null -ne $partial){$result[$phase]=$partial;continue}
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
