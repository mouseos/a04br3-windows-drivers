# Offline private copies only. Never stage a service, alter selection or contact hardware.
param([Parameter(Mandatory=$true)][string]$WorkspaceRoot,
 [Parameter(Mandatory=$true)][string]$Manifest,
 [Parameter(Mandatory=$true)][string]$ExpectedManifestSha256,
 [Parameter(Mandatory=$true)][string]$SourceDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
foreach($p in @($Manifest,$SourceDirectory,$OutputDirectory)){
 if(![IO.Path]::GetFullPath($p).StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Workspace-local private paths required'}
}
if($ExpectedManifestSha256 -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash $Manifest -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ExpectedManifestSha256){throw 'Explicit returned manifest SHA256 mismatch'}
$m=Get-Content $Manifest -Raw|ConvertFrom-Json
if($m.status -ne 'captured-unmounted' -or !$m.unmounted -or !$m.read_only -or $m.errors.Count){throw 'Completed unmounted capture required'}
if(Test-Path $OutputDirectory){throw 'New private output required'}
New-Item -ItemType Directory "$OutputDirectory\replayed"|Out-Null
$pins=[ordered]@{}
foreach($name in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){
 $entries=@($m.files|Where-Object{$_.source_path.Replace('\','/') -ceq "Windows/System32/config/$name"})
 if($entries.Count -ne 1){throw 'One exact SYSTEM input record required'}
 $e=$entries[0];$sourceFile=Join-Path $SourceDirectory $name
 if((Get-Item $sourceFile).Length -ne $e.bytes -or (Get-FileHash $sourceFile -Algorithm SHA256).Hash.ToLowerInvariant() -cne $e.sha256){throw 'SYSTEM input source mismatch'}
 Copy-Item $sourceFile "$OutputDirectory\replayed\$name"
 $pins[$name]=@{sha256=$e.sha256;bytes=$e.bytes}
}
Add-Type -Path "$PSScriptRoot\OfflineSystemHiveNative.cs"
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
$result=[ordered]@{source_manifest_sha256=$ExpectedManifestSha256;source_files=$pins;scope='private native replay; seven fixed hardware selections, read only';nodes=@{};errors=@()}
try {
 $priv=New-Object MT8167OfflineHive.Privileges
 $result.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$OutputDirectory\replayed\SYSTEM")
 if($result.load_rc){throw 'Private copy load failed'};$loaded=$true
 $root=[MT8167OfflineHive.Native]::OpenRoot($mount);$select=[MT8167OfflineHive.Native]::OpenPath($root,'Select',$false)
 try{$current=[int]$select.GetValue('Current');if($current -lt 1 -or $current -gt 999){throw 'Invalid controlset'}}finally{$select.Dispose()}
 $cs='ControlSet'+$current.ToString('000')
 $graphics=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Control\GraphicsDrivers",$false)
 $result.graphics_drivers_policy=[ordered]@{}
 try {
  foreach($name in @('DisableAutoAcpiPostDeivce','DisableAutoAcpiPostDevice','ValidateWDDMCaps')){
   $present=$graphics -and @($graphics.GetValueNames()) -contains $name
   $result.graphics_drivers_policy[$name]=if($present){@{present=$true;kind=[string]$graphics.GetValueKind($name);value=$graphics.GetValue($name)}}else{@{present=$false}}
  }
 }finally{if($graphics){$graphics.Dispose()}}
 foreach($id in @('ACPI\BEN0001','MT8167\AFE','ACPI\MOU0001','MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007','ACPI\MOU0006')){
  $base=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Enum\$id",$false);if(!$base){throw "Missing fixed node: $id"}
  try{
   $names=@($base.GetSubKeyNames());if($names.Count -gt 8){throw 'Instance bound exceeded'};$rows=@()
   foreach($name in $names){
    $key=$base.OpenSubKey($name,$false)
    try{
     $row=[ordered]@{instance=$name}
     foreach($v in @('ClassGUID','Service','Driver','ConfigFlags','DeviceDesc','HardwareID')){$row[$v]=$key.GetValue($v)}
     $driver=[string]$row.Driver
     if($driver.Length -ne 43 -or $driver[0] -ne '{' -or $driver[37] -ne '}' -or $driver[38] -ne [char]92 -or $driver.Substring(39) -notmatch '^[0-9]{4}$'){throw 'Invalid class driver path'}
     [void][Guid]::Parse($driver.Substring(0,38))
     $dk=[MT8167OfflineHive.Native]::OpenPath($root,"$cs\Control\Class\$driver",$false);if(!$dk){throw 'Missing selected class key'}
     try{
      $vals=[ordered]@{}
      foreach($v in @('InfPath','InfSection','DriverVersion','DriverDate','DriverDateData','ProviderName','MatchingDeviceId')){
       $x=$dk.GetValue($v);if($x -is [byte[]]){$x=[BitConverter]::ToString($x).Replace('-','')};$vals[$v]=$x
      }
      $row.selected=$vals
     }finally{$dk.Dispose()}
     $rows+=@($row)
    }finally{$key.Dispose()}
   }
   $result.nodes[$id]=$rows
  }finally{$base.Dispose()}
 }
}catch{$result.errors+=@($_.Exception.Message)}finally{
 if($root){$root.Dispose()};if($loaded){$result.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)}
 if($priv){$priv.Dispose();$result.privilege_restore_error=$priv.RestoreError}
}
$result|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$OutputDirectory\bindings.json"
if($result.errors.Count -or $result.load_rc -or $result.unload_rc -or $result.privilege_restore_error){throw 'Offline selection read failed'}
