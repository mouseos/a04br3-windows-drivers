# Diagnostic decoder only. Receives the existing offline reader values; never mounts or touches a device.
function ConvertFrom-Ge8300Boundaries354 {
 param([Parameter(Mandatory=$true)]$Values)
 $contract=Get-Content (Join-Path $PSScriptRoot 'ge8300-boundaries354-contract.json') -Raw|ConvertFrom-Json
 $out=[ordered]@{contract=$contract;records=[ordered]@{};hardware_success_inferred=$false;missing_means_unknown=$true}
 foreach($p in $contract.records.PSObject.Properties){
  $v=$Values.($p.Name);if(!$v -or !$v.present){$out.records[$p.Name]=@{present=$false;interpretation='unknown'};continue}
  $b=if($v.value -is [string]){[byte[]]@(for($i=0;$i -lt $v.value.Length;$i+=2){[Convert]::ToByte($v.value.Substring($i,2),16)})}else{[byte[]]$v.value}
  if($b.Length -ne 40 -or [BitConverter]::ToUInt32($b,0) -ne 2 -or [BitConverter]::ToUInt32($b,4) -ne 40 -or [BitConverter]::ToUInt32($b,8) -ne [uint32]$p.Name.Substring(1)){$out.records[$p.Name]=@{present=$true;interpretation='invalid/unknown'};continue}
  $f=[ordered]@{};for($i=0;$i -lt 4;$i++){$f[$p.Value.fields[$i]]=[BitConverter]::ToUInt32($b,16+4*$i)}
  $out.records[$p.Name]=@{present=$true;status_hex=('{0:X8}' -f [BitConverter]::ToUInt32($b,12));fields=$f;FILETIME=[BitConverter]::ToUInt64($b,32);success_inferred=$false}
 }
 return $out
}
