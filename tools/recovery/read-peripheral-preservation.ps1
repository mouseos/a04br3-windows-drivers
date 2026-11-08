# Private native clone only. No original hive load, service arm or hardware access.
param([Parameter(Mandatory=$true)][string]$Workspace,
 [Parameter(Mandatory=$true)][string]$Manifest,[Parameter(Mandatory=$true)][string]$ExpectedManifestSha256,
 [Parameter(Mandatory=$true)][int]$CaptureNumber,[Parameter(Mandatory=$true)][string]$ExpectedRecoveryBootId,
 [Parameter(Mandatory=$true)][string]$ExpectedGpuPolicySha256,
 [Parameter(Mandatory=$true)][string]$SourceDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$w=[IO.Path]::GetFullPath($Workspace).TrimEnd('\')
foreach($p in @($Manifest,$SourceDirectory,$OutputDirectory)){if(![IO.Path]::GetFullPath($p).StartsWith($w+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Workspace-local paths required'}}
if($ExpectedManifestSha256 -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash $Manifest -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ExpectedManifestSha256){throw 'Fresh manifest pin required'}
$m=Get-Content $Manifest -Raw|ConvertFrom-Json
if($CaptureNumber -lt 114 -or $ExpectedRecoveryBootId -cnotmatch '^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$' -or $ExpectedGpuPolicySha256 -cnotmatch '^[0-9a-f]{64}$' -or $m.recovery_boot_id -cne $ExpectedRecoveryBootId -or $Manifest -notmatch ('private-windows-rt-prebackup-run'+$CaptureNumber+'[\\/]manifest.json$') -or $m.status -cne 'captured-unmounted' -or !$m.unmounted -or !$m.read_only -or $m.errors.Count){throw 'Explicit main-selected capture114+ hash/boot/policy required'}
if(Test-Path $OutputDirectory){throw 'New private output required'}
New-Item -ItemType Directory "$OutputDirectory\replayed"|Out-Null
$pins=[ordered]@{}
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){
 $records=@($m.files|Where-Object{$_.source_path -ceq "Windows/System32/config/$n"});if($records.Count -ne 1){throw 'One exact trio record required'}
 $e=$records[0];if((Get-FileHash "$SourceDirectory\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $e.sha256 -or (Get-Item "$SourceDirectory\$n").Length -ne $e.bytes){throw 'Original input mismatch'}
 $pins[$n]=$e.sha256;Copy-Item "$SourceDirectory\$n" "$OutputDirectory\replayed\$n"
}
Add-Type -Path "$PSScriptRoot\OfflineSystemHiveNative.cs"
# Reuse the exact production subtree reader, without executing prepare's CLI.
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\prepare-offline-system-service.ps1",[ref]$t,[ref]$e)
if($e.Count){throw 'Production parser error'}
$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-ServiceSnapshot'},$true));if($f.Count -ne 1){throw 'One production snapshot function required'}
Invoke-Expression $f[0].Extent.Text
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
$r=[ordered]@{source_manifest_sha256=$ExpectedManifestSha256;recovery_boot_id=$ExpectedRecoveryBootId;gpu_policy_sha256=$ExpectedGpuPolicySha256;source_files=$pins;nodes=[ordered]@{};service_preservation=[ordered]@{};errors=@();load_rc=-1;unload_rc=-1;privilege_restore_error=-1}
try{
 $priv=New-Object MT8167OfflineHive.Privileges;$r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\replayed\SYSTEM");if($r.load_rc){throw 'Private replay failed'};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false);try{$cs=[int]$select.GetValue('Current');if($cs -ne 1){throw 'Explicit ControlSet001 required'}}finally{$select.Dispose()}
 foreach($n in @('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs')){$r.service_preservation[$n]=Peripheral-ServiceSnapshot $root ("ControlSet001\Services\$n")}
 # The pinned generation must exist in this same replay. This is identity evidence,
 # not an assertion that either phase completed successfully.
 $generation=[MT8167OfflineHive.Native]::OpenPath($root,("ControlSet001\Services\MT8167RenderSingleGpu\Parameters\LastDiagnostic\"+$ExpectedGpuPolicySha256),$false)
 if(!$generation){throw 'Pinned current GPU policy generation absent'}
 try{
  $phases=@($generation.GetSubKeyNames());if($generation.GetValueNames().Count -or !$phases.Count -or $phases.Count -gt 2){throw 'Unexpected pinned generation shape'}
  foreach($phase in $phases){
   if($phase -cnotin @('Install','Diagnostic')){throw 'Unknown diagnostic phase'}
   $leaf=$generation.OpenSubKey($phase,$false)
   try{if($leaf.GetSubKeyNames().Count -or $leaf.GetValueNames().Count -gt 64 -or $leaf.GetValueKind('PolicyHash') -ne [Microsoft.Win32.RegistryValueKind]::String -or $leaf.GetValue('PolicyHash') -cne $ExpectedGpuPolicySha256){throw 'Pinned GPU policy identity mismatch'}}finally{$leaf.Dispose()}
  }
 }finally{$generation.Dispose()}
 foreach($id in @('MT8167\USB20','ACPI\MOU0010','ACPI\MOU0001','ACPI\BEN0001')){
  $key=[MT8167OfflineHive.Native]::OpenPath($root,("ControlSet001\Enum\$id"),$false);if(!$key){throw 'Required existing batch target missing'}
  try{
   $instances=@($key.GetSubKeyNames());if($instances.Count -ne 1){throw 'Exactly one fixed target instance required'}
   $k=[MT8167OfflineHive.Native]::OpenPath($root,("ControlSet001\Enum\$id\"+$instances[0]),$false)
   try{
    $row=[ordered]@{instance=$instances[0]};foreach($v in @('Service','ClassGUID','Driver','HardwareID','ConfigFlags')){$row[$v]=$k.GetValue($v)}
    if([string]$row.Driver -cnotmatch '^\{[0-9a-fA-F-]{36}\}\\[0-9]{4}$'){throw 'Fixed selected class path required'}
    $dk=[MT8167OfflineHive.Native]::OpenPath($root,("ControlSet001\Control\Class\"+$row.Driver),$false)
    if(!$dk){throw 'Selected class absent'}
    try{$selected=[ordered]@{};foreach($v in @('InfPath','DriverVersion','DriverDate','MatchingDeviceId','ProviderName')){$selected[$v]=$dk.GetValue($v)};$row.selected=$selected}finally{$dk.Dispose()}
    $r.nodes[$id]=@($row)
   }finally{$k.Dispose()}
  }finally{$key.Dispose()}
 }
}catch{$r.errors+=@($_.Exception.Message)}finally{if($root){$root.Dispose()};if($loaded){$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)};if($priv){$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}}
$r.original_unchanged=$true
foreach($n in $pins.Keys){if((Get-FileHash "$SourceDirectory\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins[$n]){$r.original_unchanged=$false;$r.errors+=@('Original changed')}}
$r|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$OutputDirectory\peripheral-baseline.json"
if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.original_unchanged){throw 'Private batch baseline failed'}
