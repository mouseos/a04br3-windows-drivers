# fresh144専用。原hiveをloadせずcloneだけを読み、SCM設定・固定nodeを保存する。
param([Parameter(Mandatory=$true)][string]$WorkspaceRoot,[Parameter(Mandatory=$true)][string]$InputDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$w=[IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
foreach($p in @($InputDirectory,$OutputDirectory)){if(![IO.Path]::GetFullPath($p).StartsWith($w+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Task workspace-local paths required'}}
$out=$OutputDirectory;$input=Join-Path $InputDirectory 'trio'
if(Test-Path $out){throw 'New clone only'};New-Item -ItemType Directory "$out\clone"|Out-Null
$m=Get-Content "$InputDirectory\manifest.json" -Raw|ConvertFrom-Json
if((Get-FileHash "$InputDirectory\manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant() -cne '0faa9fd09cce293d411ad025e64e54c27d5975948b54935c00e25a3feff15a60'){throw 'Exact fresh144 manifest required'}
if($m.status -cne 'captured-unmounted' -or !$m.unmounted -or !$m.read_only -or $m.errors.Count -or $m.files.Count -ne 39 -or $m.recovery_boot_id -cne 'c35ceece-d15b-4e07-98f9-5f1d2accd64f'){throw 'Completed fresh144 capture required'}
$pins=[ordered]@{};foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){$e=@($m.files|Where-Object{$_.source_path -ceq "Windows/System32/config/$n"});if($e.Count -ne 1 -or (Get-FileHash "$input\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $e[0].sha256 -or (Get-Item "$input\$n").Length -ne $e[0].bytes){throw 'Input trio mismatch'};$pins[$n]=$e[0].sha256;Copy-Item "$input\$n" "$out\clone\$n"}
Add-Type -Path "$PSScriptRoot\OfflineSystemHiveNative.cs"
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\prepare-offline-system-service.ps1",[ref]$t,[ref]$e);if($e.Count){throw 'Parse failed'}
$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-ServiceSnapshot'},$true));if($f.Count -ne 1){throw 'Snapshot function missing'};Invoke-Expression $f[0].Extent.Text
function Values($k,[string[]]$only){
 $r=[ordered]@{};$names=@($k.GetValueNames());if($names.Count -gt 64 -or $k.GetSubKeyNames().Count -gt 32){throw 'Fixed key bound exceeded'}
 foreach($n in $only){if($n -cnotin $names){$r[$n]=@{present=$false};continue};$kind=[string]$k.GetValueKind($n);if($kind -cnotin @('String','ExpandString','MultiString','Binary','DWord','QWord','None')){throw 'Unknown fixed value type'};$v=$k.GetValue($n,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);if($v -is [byte[]]){if($v.Length -gt 65536){throw 'Value byte bound exceeded'};$v=[BitConverter]::ToString($v).Replace('-','')}elseif($v -is [string]){if($v.Length -gt 32767){throw 'String byte bound exceeded'}}elseif($v -is [string[]]){if(2*(($v -join '').Length+$v.Count+1) -gt 65536){throw 'MultiString byte bound exceeded'}};$r[$n]=@{present=$true;kind=$kind;value=$v}}
 return $r
}
$r=[ordered]@{source_manifest_sha256='0faa9fd09cce293d411ad025e64e54c27d5975948b54935c00e25a3feff15a60';source_files=$pins;old_services=[ordered]@{};services=[ordered]@{};nodes=[ordered]@{};old_gpu_service=$null;errors=@();load_rc=-1;unload_rc=-1;privilege_restore_error=-1;originals_unchanged=$false}
$root=$null;$priv=$null;$loaded=$false;$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N')
try{$priv=New-Object MT8167OfflineHive.Privileges;$r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$out\clone\SYSTEM");if($r.load_rc){throw 'Load failure'};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)
foreach($n in @('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140','MT8167CombinedFollowup142')){$r.old_services[$n]=Peripheral-ServiceSnapshot $root "ControlSet001\Services\$n"}
$r.services.Mt6392Keys=Peripheral-ServiceSnapshot $root 'ControlSet001\Services\Mt6392Keys'
 foreach($id in @('ACPI\BEN0001','MT8167\AFE','ACPI\MOU0001','MT8167\USB20','A04BR3\MT6630SDIO','ACPI\MOU0007','ACPI\MOU0006','ACPI\MOU0010','ACPI\MOU0013','MT6392\RTC','MT8167\Thermal','MT6392\Buttons','ACPI\MOU0008')){
  $k=[MT8167OfflineHive.Native]::OpenPath($root,"ControlSet001\Enum\$id",$false);if(!$k){$r.nodes[$id]=@{present=$false};continue}
  try{$rows=@();$names=@($k.GetSubKeyNames());if($names.Count -gt 8){throw 'Fixed node instance bound exceeded'};foreach($n in $names){$leaf=[MT8167OfflineHive.Native]::OpenPath($root,("ControlSet001\Enum\$id\"+$n),$false);try{$v=Values $leaf @('Service','ClassGUID','Driver','HardwareID','ConfigFlags','LowerFilters','UpperFilters');$row=[ordered]@{instance=$n;values=$v;selected_present=$false};if($v.Driver.present){$d=[string]$v.Driver.value;if($d -cnotmatch '^\{[a-fA-F0-9-]{36}\}\\[0-9]{4}$'){throw 'Unknown selected class path'};$dk=[MT8167OfflineHive.Native]::OpenPath($root,"ControlSet001\Control\Class\$d",$false);if($dk){try{$row.selected=Values $dk @('InfPath','InfSection','DriverVersion','DriverDate','MatchingDeviceId','ProviderName','LowerFilters','UpperFilters');$row.selected_present=$true}finally{$dk.Dispose()}}};$rows+=@($row)}finally{$leaf.Dispose()}};$r.nodes[$id]=@{present=$true;instances=$rows}}finally{$k.Dispose()}
 }
 $sg=[MT8167OfflineHive.Native]::OpenPath($root,"ControlSet001\Services\MT8167RenderSingleGpu",$false);if(!$sg){throw 'Retained GPU service missing'};try{$r.old_gpu_service=Values $sg @('Start','ImagePath');if($sg.GetValue('Start') -ne 4){$r.errors+=@('Old GPU service no longer Start4')}}finally{$sg.Dispose()}

}catch{$r.errors+=@($_.Exception.Message)}finally{if($root){$root.Dispose()};if($loaded){$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)};if($priv){$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}}
$r.originals_unchanged=$true;foreach($n in $pins.Keys){if((Get-FileHash "$input\$n" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins[$n]){$r.originals_unchanged=$false}}
$r|ConvertTo-Json -Depth 15|Set-Content -Encoding UTF8 "$out\receipt.json"
if($r.errors.Count -or $r.load_rc -or $r.unload_rc -or $r.privilege_restore_error -or !$r.originals_unchanged){throw 'Native baseline invalid'}
'FRESH144_SEVENTEEN_OLD_SERVICES_NATIVE_BASELINE_OK'
