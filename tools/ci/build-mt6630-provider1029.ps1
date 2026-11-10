param(
 [Parameter(Mandatory=$true)][string]$CacheDirectory,
 [Parameter(Mandatory=$true)][string]$ObjectPlan,
 [Parameter(Mandatory=$true)][string]$ObjectPlanSha256,
 [Parameter(Mandatory=$true)][string]$BaselineManifest,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
if(Test-Path $out){throw 'Fresh output required'}
if(-not $out.StartsWith($root+'\build\',[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be below source build'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
New-Item -ItemType Directory "$out\obj\provider","$out\package\provider","$out\tmp"|Out-Null
$env:TMP="$out\tmp";$env:TEMP=$env:TMP
$receipt=@{schema='mt6630-provider1029-build-receipt-v1';source_commit=$commit;status='started';hardware_verified=$false;SDK_setup=$false;parent_rebuilt=$false;GPU_rebuilt=$false;PCO_rebuilt=$false;client_rebuilt=$false;compiled=@();reused=@();error=$null}
function SaveReceipt {$receipt|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$out\build-receipt.json"}
SaveReceipt
try {
 if((Sha $BaselineManifest) -cne 'fbe1a4dc4101726d21120212ce371740421d1491c3d8a94e33369aec9a3277a2'){throw 'Actual provider1027 manifest required'}
 if((Sha $ObjectPlan) -cne $ObjectPlanSha256){throw 'Object plan pin mismatch'}
 $baseline=Get-Content $BaselineManifest -Raw|ConvertFrom-Json;$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
 if($plan.source_commit -cne $commit -or $plan.objects.Count -ne 43){throw 'Plan source/count mismatch'}
 $cache=Get-Content "$CacheDirectory\cache-manifest.json" -Raw|ConvertFrom-Json
 if($cache.source_commit -cne 'ae2e070851fdfb933416897321aec552d84e74a6' -or $cache.provider_objects -ne 43){throw 'Known cache required'}
 foreach($f in $cache.files){$p=Join-Path $CacheDirectory $f.path;if((Get-Item $p).Length -ne $f.bytes -or (Sha $p) -cne $f.sha256){throw ('Cache pin mismatch '+$f.path)}}
 $kit='C:\mt8167-public-fullsource323\wdk81\Include';$wdk="$CacheDirectory\small\wdk";$km="$wdk\Lib\winv6.3\km\arm"
 # Exact baseline target/defines/NDIS6.30 ABI; cache is restored at its original path.
 $flags=@($baseline.flags)
 if([IO.Path]::GetFullPath($CacheDirectory) -ine 'C:\mt6630-start2015-cache-ae2e0708\closure'){throw 'Original flag paths required'}
 $tools=@();foreach($name in @('clang','lld-link','llvm-readobj')){$p=(Get-Command $name -ErrorAction Stop).Source;$tools+=@{name=$name;path=$p;sha256=(Sha $p)}}
 $receipt.tools=$tools;$receipt.compiler_version=(& clang --version|Out-String);$receipt.baseline_manifest_sha256=Sha $BaselineManifest;$receipt.plan_sha256=Sha $ObjectPlan
 $records=@();$objects=@();$seen=@{}
 foreach($x in $plan.objects){
  if($seen.ContainsKey($x.source_file)){throw 'Duplicate TU'};$seen[$x.source_file]=$true
  $old=@($baseline.objects|Where-Object {$_.source -ceq $x.source_file});if($old.Count -ne 1 -or $old[0].sha256 -cne $x.cached_sha256){throw 'Baseline TU/object mismatch'}
  $changed=$false
  foreach($f in $x.source_closure.PSObject.Properties){
   $p=[IO.Path]::GetFullPath((Join-Path $root $f.Name));if(-not $p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $p) -cne $f.Value){throw ('Source closure mismatch '+$f.Name)}
   if($old[0].source_closure.($f.Name) -cne $f.Value){$changed=$true}
  }
  foreach($f in $old[0].source_closure.PSObject.Properties){if($x.source_closure.($f.Name) -cne $f.Value){$changed=$true}}
  if($changed -ne $x.compile){throw ('Compile decision mismatch '+$x.source_file)}
  $deps=$null
  if($x.compile){
   $obj=Join-Path "$out\obj" $x.object;$deps=$obj+'.d'
   & clang @flags -MMD -MF $deps -c "$root\$($x.source_file)" -o $obj
   if($LASTEXITCODE){throw ('Compile failed '+$x.source_file)}
   $receipt.compiled+=@($x.source_file)
  } else {
   $obj=Join-Path $CacheDirectory $x.cached_object
   if((Sha $obj) -cne $x.cached_sha256){throw ('Reused object mismatch '+$x.source_file)}
   $receipt.reused+=@($x.source_file)
  }
  $objects+=@($obj);$records+=@{source=$x.source_file;object=$x.object;compiled=$x.compile;sha256=(Sha $obj);source_closure=$x.source_closure;dependency_file=$deps}
  SaveReceipt
 }
 if($receipt.compiled.Count -ne $plan.expected_compile_count -or $receipt.reused.Count -ne (43-$plan.expected_compile_count)){throw 'TU count mismatch'}
 $dir="$out\package\provider";$sys="$dir\mt6630-combo-runtime.sys"
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$sys" "/map:$out\provider.map"
 if($LASTEXITCODE){throw 'Provider link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" $dir
 if(-not (Select-String -LiteralPath "$dir\mt6630-combo-runtime.inf" -SimpleMatch 'DriverVer=11/11/2026,2026.11.11.1029')){throw 'Candidate INF version mismatch'}
 $unsigned=Sha $sys
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$inf2cat="$CacheDirectory\small\signing\bin\Inf2Cat.exe"
 $receipt.signing_tools=@(@{path=$sign;sha256=(Sha $sign)},@{path=$inf2cat;sha256=(Sha $inf2cat)})
 & $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS signing failed'}
 & $inf2cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My "$dir\mt6630-combo-runtime.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($e in @('sys','cat')){$sig=Get-AuthenticodeSignature "$dir\mt6630-combo-runtime.$e";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF signature verification failed'}}
 foreach($e in @('sys','inf')){& $sign verify /pa /c "$dir\mt6630-combo-runtime.cat" "$dir\mt6630-combo-runtime.$e";if($LASTEXITCODE){throw 'CAT membership failed'}}
 & llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\provider-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
 @{schema='mt6630-provider-connect-observation1029-v1';source_commit=$commit;role='provider';version='2026.11.11.1029';unsigned_sha256=$unsigned;signer_thumbprint=$thumb;signature_status='Valid';objects=$records;flags=$flags;tools=$tools;baseline_source_commit=$baseline.source_commit;baseline_manifest_sha256=(Sha $BaselineManifest);legacy_checkpoint_build=1029;strict_cache_qualified=$false;historical_system_include_closure_available=$false;SDK_setup=$false;parent_rebuilt=$false;GPU_rebuilt=$false;PCO_rebuilt=$false;audio_rebuilt=$false;client2018_unchanged=$true;connect_observation_only=$false;held1028_data_path_included=$true;idempotent_absent_key_delete_fix=$true;direct_user_failure_cause_proven=$false;hardware_verified=$false;association_verified=$false;ip_communication_verified=$false;files=@(Get-ChildItem $dir -File|ForEach-Object{@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$dir\signed-manifest.json"
 $records|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$out\object-manifest.json"
 $receipt.status='signed-artifact-verified-not-hardware';$receipt.unsigned_SYS_sha256=$unsigned;$receipt.signed_SYS_sha256=Sha $sys;SaveReceipt
 Compress-Archive "$out\package\*" "$out\signed-packages.zip"
 Compress-Archive "$out\obj\*","$out\object-manifest.json","$out\provider-pe.txt","$out\provider.map","$out\build-receipt.json" "$out\changed-objects.zip"
 'MT6630_PROVIDER1029_SIGNED_NOT_COMMUNICATION_SUCCESS'
} catch {
 $receipt.status='failed';$receipt.error=$_.Exception.ToString();SaveReceipt;throw
}
