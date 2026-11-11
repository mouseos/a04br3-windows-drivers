# historical233だけを使うWindows native参照試験。配備input-admissionは実行しない。
[CmdletBinding()]
param(
 [switch]$ReferenceOnly,
 [Parameter(Mandatory=$true)][string]$HistoricalManifest,
 [Parameter(Mandatory=$true)][string]$TrioDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedManifestSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedSystemSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedLog1Sha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedLog2Sha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedRecipeSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedHelperSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceCommit,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if(!$ReferenceOnly){throw 'Explicit -ReferenceOnly is required; this is not deployment admission'}
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT){throw 'Windows native reference test only'}
$repo=[IO.Path]::GetFullPath("$PSScriptRoot\..")
$recipe=Join-Path $repo 'tools\recovery\prepare-usb1121-system198.ps1'
$helper=Join-Path $repo 'tools\recovery\Usb1121OfflineHiveNative.cs'
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
if(!$OutputDirectory.StartsWith($repo+'\out\',[StringComparison]::OrdinalIgnoreCase) -and !$OutputDirectory.StartsWith($repo+'\build\',[StringComparison]::OrdinalIgnoreCase)){throw 'New checkout-local out/build clone directory required'}
# junction/symlink経由でcheckout外へ出力しない。
$walk=$OutputDirectory
while($walk){
 if(Test-Path -LiteralPath $walk){$item=Get-Item -LiteralPath $walk -Force;if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'Reparse point in reference output path'}}
 $parent=[IO.Path]::GetDirectoryName($walk);if($parent -eq $walk){break};$walk=$parent
}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Reference output must not already exist'}
New-Item -ItemType Directory -Path $OutputDirectory -ErrorAction Stop|Out-Null
function Sha($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function SaveReference($name,$value){
 if($name -notlike 'reference-*'){throw 'Reference-only receipt filename required'}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory $name),($value|ConvertTo-Json -Depth 24),(New-Object Text.UTF8Encoding($false)))
}
$script:NativePhase='reference-input-validation';$script:NativeKeyPath='';$script:RawBefore=$null;$script:RawAfter=$null
$RearmExistingService=$true
$priv=$null;$root=$null;$loaded=$false;$failure=$null;$success=$null;$sourceCommit=$null
$mount='MT8167OfflineSYSTEM_'+[Guid]::NewGuid().ToString('N')
$locks=New-Object 'System.Collections.Generic.List[System.IDisposable]'
$cleanupErrors=New-Object 'System.Collections.Generic.List[string]'
$rcs=[ordered]@{load_rc=$null;first_unload_rc=$null;reload_rc=$null;final_unload_rc=$null;cleanup_unload_rc=$null;privilege_restore_error=$null}
$oldEnvironment=@{TMP=$env:TMP;TEMP=$env:TEMP;TMPDIR=$env:TMPDIR}
$originals=[ordered]@{};$originalsAfter=[ordered]@{};$rawAfter=$null;$rawReloaded=$null
$originalsUnchanged=$false;$privilegeAttempted=$false
try{
 $tmp=Join-Path $OutputDirectory 'tmp';New-Item -ItemType Directory -Path $tmp|Out-Null
 $env:TMP=$tmp;$env:TEMP=$tmp;$env:TMPDIR=$tmp
 $sourceCommit=(& git -C $repo rev-parse HEAD).Trim();if($LASTEXITCODE -or $sourceCommit -cne $ExpectedSourceCommit){throw 'Pinned source commit mismatch'}
 $dirty=@(& git -C $repo status --porcelain);if($LASTEXITCODE -or $dirty.Count){throw 'Clean committed reference source required'}
 if($ExpectedManifestSha256 -cne '80c3f36450628ac18ad21a457a7e535fb2ccf6101169f950283112c3e3fa27aa' -or $ExpectedRecipeSha256 -cne '14ebe6bddf7c34f03c7b9e9fc282b8f1294f228dd1f56c023a2f9743e7e02a22' -or $ExpectedHelperSha256 -cne 'a93142f049b9f462eb77b34fe116e5595f35810ac52162b43baa8fe1ea7f06a0'){throw 'This reference harness accepts the historical233/0772 tuple only'}
 $pins=[ordered]@{SYSTEM=$ExpectedSystemSha256;'SYSTEM.LOG1'=$ExpectedLog1Sha256;'SYSTEM.LOG2'=$ExpectedLog2Sha256}
 if($pins.SYSTEM -cne '670b1815b0472af4772f2cac5760a252ad57812729d7c33a005d69c5cea3f18e' -or $pins.'SYSTEM.LOG1' -cne '75936ef10e6da9480fc25c036fce0684469d785952d6f3847896ace3ee97e911' -or $pins.'SYSTEM.LOG2' -cne 'ad85670dcfcfc92921845a1822a02df30a34f69d5a5add528e997d2193010644'){throw 'Historical233 SYSTEM trio pin mismatch'}
 $HistoricalManifest=[IO.Path]::GetFullPath($HistoricalManifest);$TrioDirectory=[IO.Path]::GetFullPath($TrioDirectory)
 # 原本のwrite/delete共有を許可しないhandleを試験終了まで保持する。
 foreach($path in @($HistoricalManifest,$recipe,$helper)){$locks.Add([IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read))}
 if((Sha $HistoricalManifest) -cne $ExpectedManifestSha256 -or (Sha $recipe) -cne $ExpectedRecipeSha256 -or (Sha $helper) -cne $ExpectedHelperSha256){throw 'Manifest or production source byte drift'}
 $fm=Get-Content -LiteralPath $HistoricalManifest -Raw|ConvertFrom-Json
 if($fm.status -cne 'captured-unmounted' -or $fm.read_only -ne $true -or $fm.unmounted -ne $true -or @($fm.errors).Count -or $fm.recovery_boot_id -cne '97f29722-f255-4c50-aaec-51caf42f4baa'){throw 'Historical233 manifest provenance mismatch'}
 foreach($name in $pins.Keys){
  $path=Join-Path $TrioDirectory $name
  $locks.Add([IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read))
  $entry=@($fm.files|Where-Object source_path -CEQ ('Windows/System32/config/'+$name))
  $h=Sha $path
  if($entry.Count -ne 1 -or $entry[0].sha256 -cne $pins[$name] -or $h -cne $pins[$name] -or (Get-Item -LiteralPath $path).Length -ne $entry[0].bytes){throw "Historical raw trio mismatch: $name"}
  $originals[$name]=[ordered]@{path=$path;sha256=$h;bytes=$entry[0].bytes}
 }
 SaveReference 'reference-input-pins.json' ([ordered]@{schema='MT8167_USB1121_REFERENCE_TEST_INPUTS_V1';reference_only=$true;historical_manifest=$HistoricalManifest;manifest_sha256=$ExpectedManifestSha256;source_commit=$sourceCommit;recipe_sha256=$ExpectedRecipeSha256;helper_sha256=$ExpectedHelperSha256;harness_sha256=(Sha $PSCommandPath);originals=$originals;deployment_admission_executed=$false;historical_only_not_current_capture=$true})
 $script:NativePhase='extract-production-functions'
 $tokens=$null;$parseErrors=$null
 $ast=[System.Management.Automation.Language.Parser]::ParseFile($recipe,[ref]$tokens,[ref]$parseErrors)
 if($parseErrors.Count){throw ($parseErrors|Out-String)}
 foreach($name in @('Snapshot','PatchService')){
  $nodes=@($ast.FindAll({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
  if($nodes.Count -ne 1){throw "Unique production function required: $name"}
  # 本文を変換せず定義だけ取得する。recipeのtop-level admission/bodyは実行しない。
  . ([ScriptBlock]::Create($nodes[0].Extent.Text))
 }
 if('MT8167Usb1121OfflineHive.Native' -as [type]){throw 'Fresh PowerShell process required; helper type already loaded'}
 $script:NativePhase='load-production-helper'
 Add-Type -Path $helper -ErrorAction Stop
 $clone=Join-Path $OutputDirectory 'reference-clone';New-Item -ItemType Directory -Path $clone|Out-Null
 foreach($name in $pins.Keys){Copy-Item -LiteralPath $originals[$name].path -Destination (Join-Path $clone $name);if((Sha (Join-Path $clone $name)) -cne $pins[$name]){throw "Initial clone copy drift: $name"}}
 $script:NativePhase='enable-backup-restore-privileges';$privilegeAttempted=$true
 $priv=New-Object MT8167Usb1121OfflineHive.Privileges
 $script:NativePhase='load-reference-clone';$script:NativeKeyPath=''
 $rcs.load_rc=[MT8167Usb1121OfflineHive.Native]::Load($mount,(Join-Path $clone 'SYSTEM'))
 if($rcs.load_rc -ne 0){throw "Clone load rc=$($rcs.load_rc)"};$loaded=$true
 $root=[MT8167Usb1121OfflineHive.Native]::OpenRoot($mount)
 $script:NativePhase='verify-select';$script:NativeKeyPath='Select'
 $key=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,'Select',$false)
 if(!$key){throw 'Select missing'}
 try{if($key.GetValueKind('Current') -ne 'DWord' -or $key.GetValue('Current') -ne 1){throw 'ControlSet001 required'}}finally{$key.Dispose()}
 $script:NativePhase='verify-existing-raw4';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
 $key=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$script:NativeKeyPath,$false)
 if(!$key){throw 'Existing historical service missing; no service creation allowed'}
 try{$script:RawBefore=[MT8167Usb1121OfflineHive.Native]::RawValues($key);[MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($script:RawBefore,4)}finally{$key.Dispose()}
 SaveReference 'reference-raw-before.json' $script:RawBefore
 $script:NativePhase='reference-snapshot-before'
 $before=Snapshot $root;[IO.File]::WriteAllText((Join-Path $OutputDirectory 'reference-snapshot-before.json'),$before,(New-Object Text.UTF8Encoding($false)))
 $script:NativePhase='production-patch-existing'
 $patch=PatchService $root
 if(!$patch.service_existed -or $patch.before -cne $before){throw 'Production existing-only patch/snapshot mismatch'}
 $script:NativePhase='reference-snapshot-after'
 $after=Snapshot $root
 if($after -cne $before){throw 'Service/child/GraphicsDrivers/security snapshot changed beyond target Start'}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory 'reference-snapshot-after.json'),$after,(New-Object Text.UTF8Encoding($false)))
 $script:NativePhase='verify-existing-raw2';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
 $key=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$script:NativeKeyPath,$false)
 try{
  $rawAfter=[MT8167Usb1121OfflineHive.Native]::RawValues($key);$script:RawAfter=$rawAfter
  [MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($rawAfter,2)
  if([MT8167Usb1121OfflineHive.Native]::FingerprintValues($patch.raw_before,'Start') -cne [MT8167Usb1121OfflineHive.Native]::FingerprintValues($rawAfter,'Start')){throw 'Other service raw values changed'}
 }finally{$key.Dispose()}
 SaveReference 'reference-raw-after.json' $rawAfter
 $root.Flush();$root.Dispose();$root=$null
 $script:NativePhase='unload-reference-clone';$script:NativeKeyPath=''
 $rcs.first_unload_rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount)
 if($rcs.first_unload_rc -ne 0){throw "Clone unload rc=$($rcs.first_unload_rc)"};$loaded=$false
 $script:NativePhase='reload-reference-clone'
 $rcs.reload_rc=[MT8167Usb1121OfflineHive.Native]::Load($mount,(Join-Path $clone 'SYSTEM'))
 if($rcs.reload_rc -ne 0){throw "Clone reload rc=$($rcs.reload_rc)"};$loaded=$true
 $root=[MT8167Usb1121OfflineHive.Native]::OpenRoot($mount)
 $script:NativePhase='reference-snapshot-reloaded'
 $reloaded=Snapshot $root;if($reloaded -cne $before){throw 'Reloaded full snapshot mismatch'}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory 'reference-snapshot-reloaded.json'),$reloaded,(New-Object Text.UTF8Encoding($false)))
 $script:NativePhase='verify-reloaded-raw2';$script:NativeKeyPath='ControlSet001\Services\MT8167Usb1121Install198'
 $key=[MT8167Usb1121OfflineHive.Native]::OpenPath($root,$script:NativeKeyPath,$false)
 try{
  $rawReloaded=[MT8167Usb1121OfflineHive.Native]::RawValues($key);[MT8167Usb1121OfflineHive.Native]::VerifyServiceIdentity($rawReloaded,2)
  if([MT8167Usb1121OfflineHive.Native]::FingerprintValues($rawAfter,$null) -cne [MT8167Usb1121OfflineHive.Native]::FingerprintValues($rawReloaded,$null)){throw 'Reloaded service raw bytes differ'}
 }finally{$key.Dispose()}
 SaveReference 'reference-raw-reloaded.json' $rawReloaded
 $root.Dispose();$root=$null
 $script:NativePhase='final-unload-reference-clone';$script:NativeKeyPath=''
 $rcs.final_unload_rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount)
 if($rcs.final_unload_rc -ne 0){throw "Final clone unload rc=$($rcs.final_unload_rc)"};$loaded=$false
 $success=[ordered]@{schema='MT8167_USB1121_REARM_REFERENCE_TEST_V1';status='passed';reference_only=$true;historical_only_not_current_capture=$true;source_commit=$sourceCommit;manifest_sha256=$ExpectedManifestSha256;recipe_sha256=$ExpectedRecipeSha256;helper_sha256=$ExpectedHelperSha256;harness_sha256=(Sha $PSCommandPath);recovery_boot_id=$fm.recovery_boot_id;raw_before=$patch.raw_before;raw_after=$rawAfter;raw_reloaded=$rawReloaded;snapshot_after_equal=$true;snapshot_reloaded_equal=$true;snapshot_security='owner/group/DACL';snapshot_scope='ControlSet001 Services and GraphicsDrivers descendants; only target service root Start excluded';service_created=$false;only_write='existing Start REG_DWORD 4 -> 2';ACL_changes=0;deployment_admission_executed=$false;deployment_profile_generated=$false;media_result_compatible=$false;deployment_ready=$false;hardware_applied=$false;clone_SYSTEM_sha256=(Sha (Join-Path $clone 'SYSTEM'))}
}catch{
 $failure=[ordered]@{schema='MT8167_USB1121_REARM_REFERENCE_TEST_FAILURE_V1';status='failed';reference_only=$true;phase=$script:NativePhase;key_path=$script:NativeKeyPath;message=$_.Exception.Message;exception=$_.Exception.ToString();error_id=$_.FullyQualifiedErrorId;source_commit=$sourceCommit;manifest_sha256=$ExpectedManifestSha256;raw_before=$script:RawBefore;raw_after=$script:RawAfter;preservation_verified=$false;deployment_admission_executed=$false;deployment_ready=$false;hardware_applied=$false}
}finally{
 try{if($root){$root.Dispose();$root=$null}}catch{$cleanupErrors.Add('root dispose: '+$_.Exception.Message)}
 try{if($loaded){$rcs.cleanup_unload_rc=[MT8167Usb1121OfflineHive.Native]::Unload($mount);if($rcs.cleanup_unload_rc -ne 0){$cleanupErrors.Add('cleanup unload rc='+$rcs.cleanup_unload_rc)}else{$loaded=$false}}}catch{$cleanupErrors.Add('cleanup unload: '+$_.Exception.Message)}
 try{if($priv){$priv.Dispose();$rcs.privilege_restore_error=$priv.RestoreError;if($rcs.privilege_restore_error -ne 0){$cleanupErrors.Add('privilege restore rc='+$rcs.privilege_restore_error)}}}catch{$cleanupErrors.Add('privilege restore: '+$_.Exception.Message)}
 try{
  if($originals.Count -eq 3){
   $originalsUnchanged=$true
   foreach($name in $originals.Keys){$h=Sha $originals[$name].path;$originalsAfter[$name]=$h;if($h -cne $originals[$name].sha256){$originalsUnchanged=$false}}
   if(!$originalsUnchanged){$cleanupErrors.Add('Original SYSTEM trio changed')}
  }
  if($sourceCommit -and (Sha $HistoricalManifest) -cne $ExpectedManifestSha256){$cleanupErrors.Add('Historical manifest changed')}
 }catch{$originalsUnchanged=$false;$cleanupErrors.Add('original rehash: '+$_.Exception.Message)}
 foreach($handle in $locks){try{$handle.Dispose()}catch{$cleanupErrors.Add('original handle close: '+$_.Exception.Message)}}
 try{$env:TMP=$oldEnvironment.TMP;$env:TEMP=$oldEnvironment.TEMP;$env:TMPDIR=$oldEnvironment.TMPDIR}catch{$cleanupErrors.Add('TMP/TEMP restore: '+$_.Exception.Message)}
}
$cleanup=[ordered]@{schema='MT8167_USB1121_REARM_REFERENCE_TEST_CLEANUP_V1';reference_only=$true;phase=$script:NativePhase;key_path=$script:NativeKeyPath;native_return_codes=$rcs;privilege_constructor_attempted=$privilegeAttempted;hive_still_loaded=$loaded;errors=@($cleanupErrors);originals_unchanged=$originalsUnchanged;original_hashes_after=$originalsAfter;temp_environment_restored=($env:TMP -ceq $oldEnvironment.TMP -and $env:TEMP -ceq $oldEnvironment.TEMP -and $env:TMPDIR -ceq $oldEnvironment.TMPDIR)}
SaveReference 'reference-test-cleanup.json' $cleanup
if($cleanupErrors.Count -or $loaded -or !$originalsUnchanged -or !$cleanup.temp_environment_restored -or $rcs.privilege_restore_error -ne 0 -or !$success){
 if(!$failure){$failure=[ordered]@{schema='MT8167_USB1121_REARM_REFERENCE_TEST_FAILURE_V1';status='failed';reference_only=$true;phase='final-cleanup-validation';key_path=$script:NativeKeyPath;message='Reference native cleanup/original verification failed';deployment_admission_executed=$false;deployment_ready=$false;preservation_verified=$false}}
}
if($failure){$failure.cleanup=$cleanup;SaveReference 'reference-test-failure.json' $failure;throw ('REFERENCE_TEST_FAILED: '+$failure.message)}
$success.native_return_codes=$rcs;$success.originals_unchanged=$originalsUnchanged;$success.original_hashes_after=$originalsAfter;$success.temp_environment_restored=$cleanup.temp_environment_restored
SaveReference 'reference-test-result.json' $success
'USB1121_REARM_REFERENCE_TEST_PASSED_NOT_DEPLOYMENT_ADMISSION'
