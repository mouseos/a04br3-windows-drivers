param([Parameter(Mandatory=$true)][string]$WorkspaceRoot)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$base=[IO.Path]::GetFullPath($WorkspaceRoot);$w=$base;$src="$base\windows-rt\a04br3-windows-combined157-fresh156";$out="$base\post-preservation"
if(Test-Path $out){throw 'New comparison output required'}
New-Item -ItemType Directory "$out\baseline","$out\candidate"|Out-Null
$input="$base\port\xnu\out\private-windows-rt-prebackup-run156\files\Windows\System32\config"
foreach($n in @('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')){Copy-Item "$input\$n" "$out\baseline\$n"}
$native=Get-Content "$base\native-arm\result.json" -Raw|ConvertFrom-Json
if($native.status -cne 'verified-clean-system-services-only'){throw 'Native preparation must pass first'}
$candidate="$base\native-arm\candidate\SYSTEM";$candidateHash=(Get-FileHash $candidate -Algorithm SHA256).Hash.ToLowerInvariant()
if($candidateHash -cne $native.candidate.sha256){throw 'Candidate pin mismatch'}
Copy-Item $candidate "$out\candidate\SYSTEM"
Add-Type -Path "$src\tools\recovery\OfflineSystemHiveNative.cs"
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile("$src\tools\recovery\prepare-offline-system-service.ps1",[ref]$t,[ref]$e)
if($e.Count){throw 'Parser errors'}
$f=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Peripheral-ServiceSnapshot'},$true));if($f.Count -ne 1){throw 'One bounded snapshot reader required'};Invoke-Expression $f[0].Extent.Text
$ids=@('MT8167\USB20','MT8167\AFE','ACPI\MOU0001','ACPI\BEN0001','ACPI\MOU0007','A04BR3\MT6630SDIO','ACPI\MOU0006','ACPI\MOU0010','ACPI\MOU0013','MT6392\RTC','MT8167\Thermal','MT6392\Buttons','ACPI\MOU0008')
$report=[ordered]@{schema=1;candidate_sha256=$candidateHash;snapshots=[ordered]@{};errors=@();originals_unchanged=$false;equal=$false}
foreach($mode in @('baseline','candidate')){
 $mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N');$root=$null;$priv=$null;$loaded=$false
 $r=[ordered]@{load_rc=-1;unload_rc=-1;privilege_restore_error=-1;paths=[ordered]@{}}
 try{
  $priv=New-Object MT8167OfflineHive.Privileges;$r.load_rc=[MT8167OfflineHive.Native]::Load($mount,"$out\$mode\SYSTEM");if($r.load_rc){throw 'Clone load failed'};$loaded=$true;$root=[MT8167OfflineHive.Native]::OpenRoot($mount)

  foreach($id in $ids){
   $path="ControlSet001\Enum\$id";$r.paths[$path]=[ordered]@{}
   $k=[MT8167OfflineHive.Native]::OpenPath($root,$path,$false)
   if(!$k){continue}
   try{
    foreach($instance in $k.GetSubKeyNames()){
     $leaf=[MT8167OfflineHive.Native]::OpenPath($root,($path+'\'+$instance),$false)
     try{
      $row=[ordered]@{}
      foreach($v in @('Service','ClassGUID','Driver','HardwareID','ConfigFlags')){
       if($v -cin $leaf.GetValueNames()){
        $kind=[string]$leaf.GetValueKind($v)
        if($kind -cnotin @('String','MultiString','DWord')){throw 'Unexpected fixed node metadata type'}
        $row[$v]=@{kind=$kind;value=$leaf.GetValue($v)}
       }
      }
      $r.paths[$path][$instance]=$row;$driver=$leaf.GetValue('Driver')
      if($driver){
       $cp='ControlSet001\Control\Class\'+$driver
       $dk=[MT8167OfflineHive.Native]::OpenPath($root,$cp,$false)
       try{
        $selected=[ordered]@{}
        foreach($v in @('InfPath','DriverVersion','DriverDate','MatchingDeviceId','ProviderName')){
         if($v -cin $dk.GetValueNames()){$selected[$v]=@{kind=[string]$dk.GetValueKind($v);value=$dk.GetValue($v)}}
        }
        $r.paths[$cp]=$selected
       }finally{$dk.Dispose()}
      }
     }finally{$leaf.Dispose()}
    }
   }finally{$k.Dispose()}
  }

  foreach($path in @('Select','ControlSet001\Control\GraphicsDrivers','ControlSet001\Services\MT8167RenderSingleGpu','ControlSet001\Services\MT8167CombinedFollowup124','ControlSet001\Services\MT8167CombinedFollowup126','ControlSet001\Services\Mt6392Keys','ControlSet001\Services\MT8167CombinedFollowup128','ControlSet001\Services\MT8167CombinedFollowup130','ControlSet001\Services\MT8167CombinedFollowup132','ControlSet001\Services\MT8167CombinedFollowup134','ControlSet001\Services\MT8167CombinedFollowup136','ControlSet001\Services\MT8167CombinedFollowup140','ControlSet001\Services\MT8167CombinedFollowup142','ControlSet001\Services\MT8167CombinedFollowup144')){$r.paths[$path]=Peripheral-ServiceSnapshot $root $path}
 }catch{$report.errors+=@($_.Exception.Message)}finally{if($root){$root.Dispose()};if($loaded){$r.unload_rc=[MT8167OfflineHive.Native]::Unload($mount)};if($priv){$priv.Dispose();$r.privilege_restore_error=$priv.RestoreError}}
 $report.snapshots[$mode]=$r
}
$report.keypad_wdf_timestamp_policy='TimeOfLastSqmLog is recorded by read-combined138-post.ps1; full same-generation fingerprints remain strict'
$report.equal=($report.snapshots.baseline.paths|ConvertTo-Json -Depth 12 -Compress) -ceq ($report.snapshots.candidate.paths|ConvertTo-Json -Depth 12 -Compress)
$report.originals_unchanged=$true
$fm=Get-Content "$base\port\xnu\out\private-windows-rt-prebackup-run156\manifest.json" -Raw|ConvertFrom-Json
foreach($f in $fm.files){$p=Join-Path "$base\port\xnu\out\private-windows-rt-prebackup-run156" $f.backup_path;if((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -cne $f.sha256){$report.originals_unchanged=$false}}
$report|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$base\post-preservation.json"
if($report.errors.Count -or !$report.equal -or !$report.originals_unchanged){throw 'Target preservation comparison failed'}
foreach($r in $report.snapshots.Values){if($r.load_rc -or $r.unload_rc -or $r.privilege_restore_error){throw 'Native cleanup failed'}}
'FRESH126_13NODES_GRAPHICS_OLDGPU_LEDGER_IDENTICAL'
