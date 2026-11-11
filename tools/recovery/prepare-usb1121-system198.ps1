# 許可されたWindows native host専用。fresh原本はloadしない。cloneのみを変更する。
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$RearmExistingService,[string]$ExpectedInputClosureSha256)
$ErrorActionPreference='Stop'
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$script:NativePhase='input-validation';$script:NativeKeyPath='';$script:FailureRecorded=$false;$script:RawBefore=$null;$script:RawAfter=$null
function Snapshot($root){
 $rows=New-Object 'System.Collections.Generic.SortedDictionary[string,string]'
 function Visit($path,$depth){
  if($depth -gt 32 -or $rows.Count -gt 65536){throw 'bounded Services snapshot exceeded'}
  $script:NativeKeyPath=$path
  try{$k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$path,$false)}catch{throw ("snapshot open failed: $path : "+$_.Exception.Message)};if(!$k){throw 'snapshot key missing'}
  try{
   $fp=if($RearmExistingService -and $path -ieq 'ControlSet001\Services\MT8167Usb1121Install198'){[MT8167Usb1121OfflineHive.Native]::ValueFingerprintExceptStart($k)}else{[MT8167Usb1121OfflineHive.Native]::ValueFingerprint($k)}
   $rows.Add($path,$fp+':'+[MT8167Usb1121OfflineHive.Native]::SecurityFingerprint($k));foreach($n in $k.GetSubKeyNames()){if(!$RearmExistingService -and $path -ceq 'ControlSet001\Services' -and $n -ieq 'MT8167Usb1121Install198'){continue};Visit ($path+'\'+$n) ($depth+1)}}finally{$k.Dispose()}
 }
 Visit 'ControlSet001\Services' 0
 $script:NativeKeyPath='ControlSet001\Control\GraphicsDrivers'
 $k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Control\GraphicsDrivers',$false)
 try{if($k.GetValueKind('DisableAutoAcpiPostDeivce') -ne 'DWord' -or $k.GetValue('DisableAutoAcpiPostDeivce') -ne 0){throw 'POST0 required'};$null=$k}finally{$k.Dispose()}
 Visit 'ControlSet001\Control\GraphicsDrivers' 0
 return ($rows|ConvertTo-Json -Depth 5 -Compress)
}
function PatchService($root){
 $script:NativePhase='open-services-write';$script:NativeKeyPath='ControlSet001\Services'
 $services=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Services',$true)
 try{
  $exists=$services.GetSubKeyNames() -icontains 'MT8167Usb1121Install198'
  if($RearmExistingService -and !$exists){throw 'explicit existing rearm requires existing disabled service'}
  if(!$RearmExistingService -and $exists){throw 'service already exists; explicit existing rearm required'}
  $rawBefore=$null
  if($exists){
   $k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Services\MT8167Usb1121Install198',$false)
   try{$rawBefore=[MT8167Usb1121OfflineHive.Native]::RawValues($k);$script:RawBefore=$rawBefore;[MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($rawBefore,4)}finally{$k.Dispose()}
  }
  $script:NativePhase='snapshot-before'
  $before=Snapshot $root;$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
  if($RearmExistingService){$script:NativePhase='rearm-existing-start-only';$k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$script:NativeKeyPath,$true)}else{$script:NativePhase='create-new-service';$k=$services.CreateSubKey('MT8167Usb1121Install198')}
  try{
   if($RearmExistingService){$k.SetValue('Start',2,[Microsoft.Win32.RegistryValueKind]::DWord)}else{
   foreach($x in @(@('Type',16),@('Start',2),@('ErrorControl',1))){$k.SetValue($x[0],[int]$x[1],[Microsoft.Win32.RegistryValueKind]::DWord)}
   $k.SetValue('ImagePath','"C:\Windows\System32\MT8167Usb1121Install198.exe"',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('ObjectName','LocalSystem',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('DisplayName','MediaTek MT8167 USB ABI3 Installation',[Microsoft.Win32.RegistryValueKind]::String)
   };$k.Flush()
  }finally{$k.Dispose()}
 }finally{$services.Dispose()}
 return @{before=$before;raw_before=$rawBefore;service_existed=$exists}
}
$repo=[IO.Path]::GetFullPath("$PSScriptRoot\..\..");$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
if(!$OutputDirectory.StartsWith($repo+'\out\',[StringComparison]::OrdinalIgnoreCase) -and !$OutputDirectory.StartsWith($repo+'\build\',[StringComparison]::OrdinalIgnoreCase)){throw 'private checkout-local clone output required'}
if(Test-Path $OutputDirectory){throw 'new private output required'}
if($RearmExistingService){
 $sourceCommit=(& git -C $repo rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'native source commit unavailable'}
 $sourceStatus=@(& git -C $repo status --porcelain);if($LASTEXITCODE -or $sourceStatus.Count){throw 'clean committed native source required'}
}
if($RearmExistingService -and (!$ExpectedInputClosureSha256 -or (Sha "$Inputs\input-closure.json") -cne $ExpectedInputClosureSha256)){throw 'existing rearm requires exact current input closure pin'}
$c=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
if($RearmExistingService){
 $p=Get-Content "$Inputs\payload\profile.json" -Raw|ConvertFrom-Json
 $fm=Get-Content "$Inputs\provenance\fresh-manifest.json" -Raw|ConvertFrom-Json
 $nr=Get-Content "$Inputs\provenance\native-receipt.json" -Raw|ConvertFrom-Json
 if($p.schema -cne 'MT8167_USB1121_REBOUND_SERVICE_V2' -or $p.integration_phase -cne 'standalone-retained-gpu' -or $p.gpu_update -ne $false -or $p.current_gpu_capture_verified -ne $true -or $c.gpu_update -ne $false -or $c.integration_phase -cne $p.integration_phase -or $c.binding_schema -cne $p.schema -or $p.native_receipt_sha256 -cne $c.native_sha256 -or $p.fresh_sha256 -cne $c.fresh_sha256 -or (Sha "$Inputs\payload\profile.json") -cne $c.profile_sha256 -or (Sha "$Inputs\provenance\fresh-manifest.json") -cne $c.fresh_sha256 -or (Sha "$Inputs\provenance\native-receipt.json") -cne $c.native_sha256 -or $nr.source_manifest_sha256 -cne $c.fresh_sha256 -or $fm.recovery_boot_id -cne $p.recovery_boot_id -or $fm.recovery_boot_id -cne $nr.recovery_boot_id -or $fm.status -cne 'captured-unmounted' -or $fm.read_only -ne $true -or $fm.unmounted -ne $true -or @($fm.errors).Count){throw 'same actual capture standalone profile/provenance required'}
 foreach($f in $c.files.PSObject.Properties){$q=[IO.Path]::GetFullPath((Join-Path $Inputs $f.Name));if(!$q.StartsWith([IO.Path]::GetFullPath($Inputs).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $q) -cne $f.Value){throw 'current input leaf drift'}}
 foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){$v=@($fm.files|Where-Object source_path -CEQ ('Windows/System32/config/'+$n));if($v.Count -ne 1 -or $v[0].sha256 -cne (Sha "$Inputs\fresh-system\$n")){throw 'actual SYSTEM trio witness mismatch'}}
}
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){if((Sha "$Inputs\fresh-system\$n") -cne $c.files.("fresh-system/$n")){throw 'fresh SYSTEM trio drift'}}
New-Item -ItemType Directory $OutputDirectory|Out-Null
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){Copy-Item "$Inputs\fresh-system\$n" $OutputDirectory}
$priv=$null;$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$loaded=$false;$root=$null
try{
 $script:NativePhase='enable-backup-restore-privileges'
 Add-Type -Path "$PSScriptRoot\Usb1121OfflineHiveNative.cs"
 $priv=New-Object MT8167Usb1121OfflineHive.Privileges
 $script:NativePhase='load-clone';$script:NativeKeyPath=''
 $rc=[MT8167Usb1121OfflineHive.Native]::Load($mount,"$OutputDirectory\SYSTEM");if($rc){throw "load rc=$rc"};$loaded=$true;$root=[MT8167Usb1121OfflineHive.Native]::OpenRoot($mount)
 $script:NativePhase='select';$script:NativeKeyPath='Select'
 $sel=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'Select',$false);try{if($sel.GetValue('Current') -ne 1){throw 'ControlSet001 required'}}finally{$sel.Dispose()}
 $patch=PatchService $root;$before=$patch.before;$rawBefore=$patch.raw_before
 $script:NativePhase='snapshot-after'
 $after=Snapshot $root;if($before -cne $after){throw 'old service/GraphicsDrivers changed'};$root.Flush();$root.Dispose();$root=$null
 $rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount);if($rc){throw "unload rc=$rc"};$loaded=$false
 # 再loadしたcloneでも同一の全既存Services fingerprintを確認する。
 $rc=[MT8167Usb1121OfflineHive.Native]::Load($mount,"$OutputDirectory\SYSTEM");if($rc){throw "verify load rc=$rc"};$loaded=$true;$root=[MT8167Usb1121OfflineHive.Native]::OpenRoot($mount)
 $script:NativePhase='snapshot-reloaded'
 if((Snapshot $root) -cne $before){throw 'persisted services/POST changed'}
 $script:NativePhase='verify-new-service';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
 $k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Services\MT8167Usb1121Install198',$false)
 try{
  $rawAfter=[MT8167Usb1121OfflineHive.Native]::RawValues($k);$script:RawAfter=$rawAfter
  [MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($rawAfter,2)
  if(!$RearmExistingService -and ($k.GetValueNames().Count -ne 6 -or $k.GetSubKeyNames().Count)){throw 'persisted new service mismatch'}
 }finally{$k.Dispose()}
 $root.Dispose();$root=$null;$rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount);if($rc){throw "verify unload rc=$rc"};$loaded=$false
 foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){if((Sha "$Inputs\fresh-system\$n") -cne $c.files.("fresh-system/$n")){throw 'original drift'}}
 $r=[ordered]@{schema='MT8167_USB1121_NATIVE198_V1';fresh_sha256=$c.fresh_sha256;input_closure_sha256=(Sha "$Inputs\input-closure.json");SYSTEM_sha256=(Sha "$OutputDirectory\SYSTEM");old_services_and_GraphicsDrivers_equal=$true;originals_unchanged=$true;new_service='MT8167Usb1121Install198';native_helper_sha256=(Sha "$PSScriptRoot\Usb1121OfflineHiveNative.cs");native_recipe_sha256=(Sha $PSCommandPath);backup_restore_open=$true;ACL_changes=0;boot_GPT_changes=0;POST_value=0;hardware_applied=$false}
 if($RearmExistingService){
  $r.schema='MT8167_USB1121_NATIVE_REARM_V2';$r.Remove('new_service');$r.existing_service='MT8167Usb1121Install198'
  $r.mode='existing-disabled-start-only';$r.service_created=$false;$r.raw_before=$rawBefore;$r.raw_after=$rawAfter
  $r.preservation_exclusion='ControlSet001\Services\MT8167Usb1121Install198:Start only'
  $r.snapshot_before_utf8_base64=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($before))
  $r.snapshot_after_equal=$true;$r.snapshot_reloaded_equal=$true;$r.subkeys_preserved=$true;$r.service_other_values_preserved=$true
  $r.snapshot_security_included=$true;$r.integration_phase=$p.integration_phase;$r.profile_sha256=$c.profile_sha256
  $r.native_receipt_sha256=$c.native_sha256;$r.recovery_boot_id=$p.recovery_boot_id
 }
 $r.source_commit=(& git -C $repo rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'native recipe source identity unavailable'}
 if(!$RearmExistingService){$r.mode='create-new-service';$r.service_created=$true;$r.raw_before=$null;$r.raw_after=$rawAfter}
 $r.native_patch_complete=$true;$r.load_rc=0;$r.reload_rc=0;$r.unload_rc=0

}catch{
 $failure=[ordered]@{schema='MT8167_USB1121_NATIVE_FAILURE_V1';mode=$(if($RearmExistingService){'existing-disabled-start-only'}else{'create-new-service'});raw_before=$script:RawBefore;raw_after=$script:RawAfter;phase=$script:NativePhase;key_path=$script:NativeKeyPath;message=$_.Exception.Message;error_id=$_.FullyQualifiedErrorId;fresh_sha256=$c.fresh_sha256;clone_directory=$OutputDirectory;ACL_changes=0;unreadable_keys_skipped=0;preservation_verified=$false;hardware_applied=$false}
 [IO.File]::WriteAllText("$OutputDirectory\failure-receipt.json",($failure|ConvertTo-Json -Depth 5),(New-Object Text.UTF8Encoding($false)));$script:FailureRecorded=$true
 throw
}finally{
 $cleanup=0;$restore=0
 try{if($root){$root.Dispose()};if($loaded){$cleanup=[MT8167Usb1121OfflineHive.Native]::Unload($mount)}}finally{if($priv){$priv.Dispose();$restore=$priv.RestoreError}}
 if($cleanup -or $restore){
  $f=[ordered]@{schema='MT8167_USB1121_NATIVE_FAILURE_V1';phase='cleanup';key_path=$script:NativeKeyPath;unload_rc=$cleanup;privilege_restore_error=$restore;preservation_verified=$false;ACL_changes=0;unreadable_keys_skipped=0}
  [IO.File]::WriteAllText("$OutputDirectory\cleanup-failure-receipt.json",($f|ConvertTo-Json -Depth 5),(New-Object Text.UTF8Encoding($false)))
  throw "clone cleanup failed: unload=$cleanup privilege_restore=$restore"
 }
}
$r.privilege_restore_error=$restore
[IO.File]::WriteAllText("$OutputDirectory\result.json",($r|ConvertTo-Json -Depth 15),(New-Object Text.UTF8Encoding($false)))
