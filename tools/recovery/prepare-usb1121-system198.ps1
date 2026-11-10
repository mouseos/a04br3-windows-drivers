# main Windows VM専用。fresh原本はloadしない。cloneへの新service追加だけ。
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Snapshot($root){
 $rows=New-Object 'System.Collections.Generic.SortedDictionary[string,string]'
 function Visit($path,$depth){
  if($depth -gt 32 -or $rows.Count -gt 65536){throw 'bounded Services snapshot exceeded'}
  $k=[MT8167OfflineHive.Native]::OpenPath($root,$path,$false);if(!$k){throw 'snapshot key missing'}
  try{$rows.Add($path,[MT8167OfflineHive.Native]::ValueFingerprint($k));foreach($n in $k.GetSubKeyNames()){if($path -ceq 'ControlSet001\Services' -and $n -ceq 'MT8167Usb1121Install198'){continue};Visit ($path+'\'+$n) ($depth+1)}}finally{$k.Dispose()}
 }
 Visit 'ControlSet001\Services' 0
 $k=[MT8167OfflineHive.Native]::OpenPath($root,'ControlSet001\Control\GraphicsDrivers',$false)
 try{if($k.GetValueKind('DisableAutoAcpiPostDeivce') -ne 'DWord' -or $k.GetValue('DisableAutoAcpiPostDeivce') -ne 0){throw 'POST0 required'};$rows.Add('GraphicsDrivers',[MT8167OfflineHive.Native]::ValueFingerprint($k))}finally{$k.Dispose()}
 return ($rows|ConvertTo-Json -Depth 5 -Compress)
}
if(Test-Path $OutputDirectory){throw 'new private output required'}
$c=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){if((Sha "$Inputs\fresh-system\$n") -cne $c.files.("fresh-system/$n")){throw 'fresh SYSTEM trio drift'}}
New-Item -ItemType Directory $OutputDirectory|Out-Null
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){Copy-Item "$Inputs\fresh-system\$n" $OutputDirectory}
Add-Type -Path "$PSScriptRoot\OfflineSystemHiveNative.cs"
$priv=New-Object MT8167OfflineHive.Privileges;$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$loaded=$false;$root=$null
try{
 $rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\SYSTEM");if($rc){throw "load rc=$rc"};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 $sel=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false);try{if($sel.GetValue('Current') -ne 1){throw 'ControlSet001 required'}}finally{$sel.Dispose()}
 $services=[MT8167OfflineHive.Native]::OpenPath($root,'ControlSet001\Services',$true)
 try{if($services.GetSubKeyNames() -icontains 'MT8167Usb1121Install198'){throw 'service already exists'}
  $before=Snapshot $root;$k=$services.CreateSubKey('MT8167Usb1121Install198')
  try{
   foreach($x in @(@('Type',16),@('Start',2),@('ErrorControl',1))){$k.SetValue($x[0],[int]$x[1],[Microsoft.Win32.RegistryValueKind]::DWord)}
   $k.SetValue('ImagePath','"C:\Windows\System32\MT8167Usb1121Install198.exe"',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('ObjectName','LocalSystem',[Microsoft.Win32.RegistryValueKind]::String)
   $k.SetValue('DisplayName','MediaTek MT8167 USB ABI3 Installation',[Microsoft.Win32.RegistryValueKind]::String);$k.Flush()
  }finally{$k.Dispose()}
 }finally{$services.Dispose()}
 $after=Snapshot $root;if($before -cne $after){throw 'old service/GraphicsDrivers changed'};$root.Flush();$root.Dispose();$root=$null
 $rc=[MT8167OfflineHive.Native]::Unload($mount);if($rc){throw "unload rc=$rc"};$loaded=$false
 # 再loadしたcloneでも同一の全既存Services fingerprintを確認する。
 $rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\SYSTEM");if($rc){throw "verify load rc=$rc"};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
 if((Snapshot $root) -cne $before){throw 'persisted services/POST changed'}
 $k=[MT8167OfflineHive.Native]::OpenPath($root,'ControlSet001\Services\MT8167Usb1121Install198',$false)
 try{if($k.GetValue('Type') -ne 16 -or $k.GetValue('Start') -ne 2 -or $k.GetValue('ObjectName') -cne 'LocalSystem' -or $k.GetValue('ImagePath') -cne '"C:\Windows\System32\MT8167Usb1121Install198.exe"' -or $k.GetValueNames().Count -ne 6){throw 'persisted new service mismatch'}}finally{$k.Dispose()}
 $root.Dispose();$root=$null;$rc=[MT8167OfflineHive.Native]::Unload($mount);if($rc){throw "verify unload rc=$rc"};$loaded=$false
 foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){if((Sha "$Inputs\fresh-system\$n") -cne $c.files.("fresh-system/$n")){throw 'original drift'}}
 $r=[ordered]@{schema='MT8167_USB1121_NATIVE198_V1';fresh_sha256=$c.fresh_sha256;input_closure_sha256=(Sha "$Inputs\input-closure.json");SYSTEM_sha256=(Sha "$OutputDirectory\SYSTEM");old_services_and_GraphicsDrivers_equal=$true;originals_unchanged=$true;new_service='MT8167Usb1121Install198';boot_GPT_changes=0;POST_value=0;hardware_applied=$false}
 [IO.File]::WriteAllText("$OutputDirectory\result.json",($r|ConvertTo-Json -Depth 10),(New-Object Text.UTF8Encoding($false)))
}finally{if($root){$root.Dispose()};if($loaded){$rc=[MT8167OfflineHive.Native]::Unload($mount);if($rc){Write-Error "cleanup unload rc=$rc"}};$priv.Dispose();if($priv.RestoreError){throw 'privilege restoration failed'}}
