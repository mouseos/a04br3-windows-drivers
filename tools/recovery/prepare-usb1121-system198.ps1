# main Windows VM専用。fresh原本はloadしない。cloneへの新service追加だけ。
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$script:NativePhase='input-validation';$script:NativeKeyPath='';$script:FailureRecorded=$false
function Snapshot($root){
 $rows=New-Object 'System.Collections.Generic.SortedDictionary[string,string]'
 function Visit($path,$depth){
  if($depth -gt 32 -or $rows.Count -gt 65536){throw 'bounded Services snapshot exceeded'}
  $script:NativeKeyPath=$path
  try{$k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$path,$false)}catch{throw ("snapshot open failed: $path : "+$_.Exception.Message)};if(!$k){throw 'snapshot key missing'}
  try{$rows.Add($path,[MT8167Usb1121OfflineHive.Native]::ValueFingerprint($k));foreach($n in $k.GetSubKeyNames()){if($path -ceq 'ControlSet001\Services' -and $n -ceq 'MT8167Usb1121Install198'){continue};Visit ($path+'\'+$n) ($depth+1)}}finally{$k.Dispose()}
 }
 Visit 'ControlSet001\Services' 0
 $script:NativeKeyPath='ControlSet001\Control\GraphicsDrivers'
 $k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Control\GraphicsDrivers',$false)
 try{if($k.GetValueKind('DisableAutoAcpiPostDeivce') -ne 'DWord' -or $k.GetValue('DisableAutoAcpiPostDeivce') -ne 0){throw 'POST0 required'};$rows.Add('GraphicsDrivers',[MT8167Usb1121OfflineHive.Native]::ValueFingerprint($k))}finally{$k.Dispose()}
 return ($rows|ConvertTo-Json -Depth 5 -Compress)
}
$repo=[IO.Path]::GetFullPath("$PSScriptRoot\..\..");$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
if(!$OutputDirectory.StartsWith($repo+'\out\',[StringComparison]::OrdinalIgnoreCase) -and !$OutputDirectory.StartsWith($repo+'\build\',[StringComparison]::OrdinalIgnoreCase)){throw 'private checkout-local clone output required'}
if(Test-Path $OutputDirectory){throw 'new private output required'}
$c=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
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
 $script:NativePhase='open-services-write';$script:NativeKeyPath='ControlSet001\Services'
 $services=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Services',$true)
 try{if($services.GetSubKeyNames() -icontains 'MT8167Usb1121Install198'){throw 'service already exists'}
  $script:NativePhase='snapshot-before'
  $before=Snapshot $root;$script:NativePhase='create-new-service';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198';$k=$services.CreateSubKey('MT8167Usb1121Install198')
  try{
   foreach($x in @(@('Type',16),@('Start',2),@('ErrorControl',1))){$k.SetValue($x[0],[int]$x[1],[Microsoft.Win32.RegistryValueKind]::DWord)}
   $k.SetValue('ImagePath','"C:\Windows\System32\MT8167Usb1121Install198.exe"',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('ObjectName','LocalSystem',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('DisplayName','MediaTek MT8167 USB ABI3 Installation',[Microsoft.Win32.RegistryValueKind]::String);$k.Flush()
  }finally{$k.Dispose()}
 }finally{$services.Dispose()}
 $script:NativePhase='snapshot-after'
 $after=Snapshot $root;if($before -cne $after){throw 'old service/GraphicsDrivers changed'};$root.Flush();$root.Dispose();$root=$null
 $rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount);if($rc){throw "unload rc=$rc"};$loaded=$false
 # 再loadしたcloneでも同一の全既存Services fingerprintを確認する。
 $rc=[MT8167Usb1121OfflineHive.Native]::Load($mount,"$OutputDirectory\SYSTEM");if($rc){throw "verify load rc=$rc"};$loaded=$true;$root=[MT8167Usb1121OfflineHive.Native]::OpenRoot($mount)
 $script:NativePhase='snapshot-reloaded'
 if((Snapshot $root) -cne $before){throw 'persisted services/POST changed'}
 $script:NativePhase='verify-new-service';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
 $k=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'ControlSet001\Services\MT8167Usb1121Install198',$false)
 try{if($k.GetValue('Type') -ne 16 -or $k.GetValue('Start') -ne 2 -or $k.GetValue('ObjectName') -cne 'LocalSystem' -or $k.GetValue('ImagePath') -cne '"C:\Windows\System32\MT8167Usb1121Install198.exe"' -or $k.GetValueNames().Count -ne 6){throw 'persisted new service mismatch'}}finally{$k.Dispose()}
 $root.Dispose();$root=$null;$rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount);if($rc){throw "verify unload rc=$rc"};$loaded=$false
 foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){if((Sha "$Inputs\fresh-system\$n") -cne $c.files.("fresh-system/$n")){throw 'original drift'}}
 $r=[ordered]@{schema='MT8167_USB1121_NATIVE198_V1';fresh_sha256=$c.fresh_sha256;input_closure_sha256=(Sha "$Inputs\input-closure.json");SYSTEM_sha256=(Sha "$OutputDirectory\SYSTEM");old_services_and_GraphicsDrivers_equal=$true;originals_unchanged=$true;new_service='MT8167Usb1121Install198';native_helper_sha256=(Sha "$PSScriptRoot\Usb1121OfflineHiveNative.cs");native_recipe_sha256=(Sha $PSCommandPath);backup_restore_open=$true;ACL_changes=0;boot_GPT_changes=0;POST_value=0;hardware_applied=$false}
 [IO.File]::WriteAllText("$OutputDirectory\result.json",($r|ConvertTo-Json -Depth 10),(New-Object Text.UTF8Encoding($false)))
}catch{
 $failure=[ordered]@{schema='MT8167_USB1121_NATIVE_FAILURE_V1';phase=$script:NativePhase;key_path=$script:NativeKeyPath;message=$_.Exception.Message;error_id=$_.FullyQualifiedErrorId;fresh_sha256=$c.fresh_sha256;clone_directory=$OutputDirectory;ACL_changes=0;unreadable_keys_skipped=0;preservation_verified=$false;hardware_applied=$false}
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
