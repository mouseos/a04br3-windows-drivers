# Read only known private SYSTEM observations. Never touches a device/live service.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedSystemSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedLog1Sha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedLog2Sha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedPolicyHash,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$BackupManifestSha256,
 [ValidateSet('run47-sdon','legacy-five','legacy-gpu-six')][string]$Profile='run47-sdon'
)
$ErrorActionPreference='Stop'
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or [IntPtr]::Size -ne 8){throw '64-bit Windows native loader required'}
$source=[IO.Path]::GetFullPath($SourceDirectory);$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Fresh receipt output required'}
if($out.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or $out -ceq $source){throw 'Output must not be inside original input'}
New-Item -ItemType Directory -Path $out|Out-Null
$work=Join-Path $out 'private-replay';New-Item -ItemType Directory $work|Out-Null
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$env:TEMP=$out;$env:TMP=$out
function HashFile($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function FixedDriverValues($Root,$ControlSet,[ValidateSet('Mt6630Runtime','Mt8167Audio')][string]$Service){
 $key=[MT8167OfflineHive.Native]::OpenPath($Root,"$ControlSet\Services\$Service\Parameters"+$(if($Service -eq 'Mt6630Runtime'){'\NativeCheckpoint'}else{''}),$false)
 if(!$key){return @{present=$false}}
 try {
  $names=if($Service -eq 'Mt6630Runtime'){@('Generation','DriverEntry','Initialize')}else{@('NativeStartComplete','NativeStart')}
  $values=[ordered]@{}
  foreach($name in $names){
   $value=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
   if($null -eq $value){$values[$name]=@{available=$false};continue}
   $kind=[string]$key.GetValueKind($name)
   if($name -in @('DriverEntry','Initialize','NativeStart')){
    $expected=if($name -ceq 'NativeStart'){56}else{1624}
    if($kind -cne 'Binary' -or $value.Length -ne $expected){throw 'Fixed driver record type/size mismatch'}
    $value=[BitConverter]::ToString($value).Replace('-','')
   }elseif(($name -ceq 'Generation' -and $kind -cne 'QWord') -or ($name -ceq 'NativeStartComplete' -and $kind -cne 'DWord')){throw 'Fixed driver scalar type mismatch'}
   $values[$name]=@{available=$true;kind=$kind;value=$value}
  }
  return @{present=$true;values=$values}
 }finally{$key.Dispose()}
}
$expected=@{SYSTEM=$ExpectedSystemSha256;'SYSTEM.LOG1'=$ExpectedLog1Sha256;'SYSTEM.LOG2'=$ExpectedLog2Sha256}
$result=[ordered]@{schema=1;status='failed';scope='fixed Engine LastDiagnostic + MT6630 NativeCheckpoint + Audio NativeStart only';source_read_only=$true;whole_system_audit=$false;live_query=$false;backup_manifest_sha256=$BackupManifestSha256;policy_hash=$ExpectedPolicyHash;profile=$Profile;source_files=$expected;engine=[ordered]@{};wifi=@{present=$false};audio=@{present=$false};errors=@();load_rc=$null;unload_rc=$null;privilege_restore_error=$null}
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
try {
 foreach($name in $expected.Keys){$p=Join-Path $source $name;if((HashFile $p) -cne $expected[$name]){throw 'Original private input pin mismatch'};[IO.File]::Copy($p,(Join-Path $work $name),$false)}
 if(-not ('MT8167OfflineHive.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'OfflineSystemHiveNative.cs')}
 . (Join-Path $PSScriptRoot 'Read-EngineLastDiagnostic.ps1')
 $priv=New-Object MT8167OfflineHive.Privileges
 $result.load_rc=[MT8167OfflineHive.Native]::Load($mount,(Join-Path $work 'SYSTEM'))
 if($result.load_rc){throw 'Private native SYSTEM replay failed'};$loaded=$true
 $root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false)
 if(!$select){throw 'Missing Select'}
 try{if($select.GetValueKind('Current') -ne [Microsoft.Win32.RegistryValueKind]::DWord){throw 'Select type mismatch'};$current=[int]$select.GetValue('Current');if($current -lt 1 -or $current -gt 999){throw 'Select range mismatch'}}finally{$select.Dispose()}
 $cs='ControlSet'+$current.ToString('000');$result.control_set=$cs
 $result.engine=Read-Mt8167EngineLastDiagnostic $root $cs $ExpectedPolicyHash
 foreach($phase in @('Install','Diagnostic')){
  $r=$result.engine[$phase];if(!$r.present){continue};$v=$r.values
  $count=if($Profile -ceq 'legacy-five'){5}else{6}
  if($v.TargetCount.value -ne $count -or ($Profile -ceq 'run47-sdon' -and ($v.Schema.value -ne 2 -or $v.Node5HardwareId.value -cne 'ACPI\MOU0013'))){throw 'Expected fixed receipt profile mismatch'}
 }
 $service=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Services\MT8167EngineProbe",$false)
 if(!$service){$result.engine_service=@{present=$false}}
 else {try {
  $fixed=@('Type','Start','ErrorControl','ImagePath','ObjectName','DisplayName','DependOnService')
  $sv=[ordered]@{}
  foreach($name in $fixed){
   $v=$service.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
   if($null -eq $v){$sv[$name]=@{available=$false};continue}
   $sv[$name]=@{available=$true;kind=[string]$service.GetValueKind($name);value=$v}
  }
  $result.engine_service=@{present=$true;values=$sv;value_count=$service.GetValueNames().Count;raw_fingerprint=[MT8167OfflineHive.Native]::ValueFingerprint($service);scope='seven fixed SCM values, no service mutation'}
 }finally{$service.Dispose()}}
 # Exactly the SD host Enum instance. Never enumerate Enum or protected Properties.
 $sd=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\ACPI\MOU0013\0",$false)
 if(!$sd){$result.sd_enum=@{present=$false;scope='fixed ACPI\MOU0013\0 only'}}
 else {try {
  $ev=[ordered]@{}
  foreach($name in @('HardwareID','CompatibleIDs','ClassGUID','Driver','Service','ConfigFlags','Problem','StatusFlags')){
   try {$v=$sd.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);if($null -eq $v){$ev[$name]=@{available=$false}}else{$ev[$name]=@{available=$true;kind=[string]$sd.GetValueKind($name);value=$v}}}
   catch {$ev[$name]=@{available=$false;error=$_.Exception.Message}}
  }
  $result.sd_enum=@{present=$true;values=$ev;live_cm_query=$false;properties_read=$false;scope='fixed Enum raw values only, missing Problem is unavailable'}
 }finally{$sd.Dispose()}}
 $result.wifi=FixedDriverValues $root $cs 'Mt6630Runtime';$result.audio=FixedDriverValues $root $cs 'Mt8167Audio'
}catch{$result.errors+=@($_.Exception.Message)}
finally {
 if($root){$root.Dispose()}
 if($loaded){$result.unload_rc=[MT8167OfflineHive.Native]::Unload($mount);if($result.unload_rc){$result.errors+=@('Private hive unload failed')}}
 if($priv){$priv.Dispose();$result.privilege_restore_error=$priv.RestoreError;if($priv.RestoreError){$result.errors+=@('Privilege restore failed')}}
 foreach($name in $expected.Keys){try{if((HashFile (Join-Path $source $name)) -cne $expected[$name]){$result.errors+=@('Original input changed')}}catch{$result.errors+=@('Original input post-check failed')}}
 $env:TEMP=$priorTemp;$env:TMP=$priorTmp
}
if(!$result.errors.Count -and $result.load_rc -eq 0 -and $result.unload_rc -eq 0 -and $result.privilege_restore_error -eq 0){$result.status='fixed-receipt-read-unloaded'}
$path=Join-Path $out 'receipt.json';[IO.File]::WriteAllText($path,($result|ConvertTo-Json -Depth 12),(New-Object Text.UTF8Encoding($false)))
@{status=$result.status;receipt_sha256=(HashFile $path)}|ConvertTo-Json
if($result.status -cne 'fixed-receipt-read-unloaded'){exit 1}
