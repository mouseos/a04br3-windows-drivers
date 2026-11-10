param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$InputsSha256,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory);$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean public source required'}
if((Test-Path $out) -or !$out.StartsWith($root+'\build\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $Inputs) -cne $InputsSha256){throw 'Pinned inputs/new checkout-local output required'}
$i=Get-Content $Inputs -Raw|ConvertFrom-Json
if($i.schema -cne 'MT6630_PROVIDER1030_POSTCATALOG_FAILURE_INPUTS_V1' -or $i.compile_source_commit -cne '2b2e2b68e3868ae2c6e6abb29efa490ca4e0812e' -or $i.baseline_manifest_sha256 -cne 'd974239fbcf96d7baad96b84fccaa7462a7481058a7c2d80c0d19d4745ef1599'){throw 'Actual prior attempt only'}
& git -C $root merge-base --is-ancestor $i.compile_source_commit $commit;if($LASTEXITCODE){throw 'Prior compile source must be ancestor'}
$pinned=@(@{path='C:\mt6630-provider1029\source\build\provider1029\package\provider\signed-manifest.json';sha256=$i.baseline_manifest_sha256},$i.prior_receipt,$i.plan,$i.signed_SYS)+@($i.dependencies)+@($i.kernel_libraries)+@($i.tools)
foreach($x in $i.objects){$pinned+=@(@{path=$x.object_path;sha256=$x.sha256})}
function CheckInputs {
 if((Sha $Inputs) -cne $InputsSha256){throw 'Input snapshot changed'}
 foreach($x in $pinned){if((Sha $x.path) -cne $x.sha256){throw 'Prior artifact changed'}}
 foreach($x in $i.objects){foreach($f in $x.source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Prior compiled closure incompatible'}}}
}
CheckInputs
$b=Get-Content $i.prior_receipt.path -Raw|ConvertFrom-Json
if($b.status -cne 'failed' -or $b.error -cnotmatch '(^|: )CAT generation failed$' -or $b.compiled.Count -ne 3 -or $b.reused.Count -ne 40 -or $b.link_count -ne 1 -or !$b.imports_equal_actual1029 -or $i.objects.Count -ne 43 -or @($i.objects|Where-Object {$_.compiled_in_prior_attempt}).Count -ne 3){throw 'Actual compile3/link1/import boundary required'}
$priorSig=Get-AuthenticodeSignature $i.signed_SYS.path;if($priorSig.Status -ne 'Valid' -or $priorSig.SignerCertificate.Thumbprint -cne $thumb){throw 'Signed SYS from real prior link required'}
New-Item -ItemType Directory "$out\package\provider","$out\evidence","$out\incremental-objects"|Out-Null
$dir="$out\package\provider";$sys="$dir\mt6630-combo-runtime.sys";$cat="$dir\mt6630-combo-runtime.cat"
$receipt=@{schema='mt6630-provider1030-catalog-continuation-v1';source_commit=$commit;compiled_source_commit=$i.compile_source_commit;input_snapshot_sha256=$InputsSha256;compiled_this_attempt=0;linked_this_attempt=0;SYS_signed_this_attempt=0;CAT_signed_this_attempt=0;prior_compiles=3;prior_reused_objects=40;prior_links=1;SDK_setup=$false;PCO_rebuilt=$false;device_touched=$false;hardware_verified=$false;success=$false}
try {
 Copy-Item -LiteralPath $i.signed_SYS.path -Destination $sys
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" $dir
 if(!(Select-String -LiteralPath "$dir\mt6630-combo-runtime.inf" -SimpleMatch 'DriverVer=11/10/2026,2026.11.11.1030')){throw 'Nonfuture formal1030 INF required'}
 $sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$inf2cat='C:\mt6630-start2015-cache-ae2e0708\closure\small\signing\bin\Inf2Cat.exe';$pins=Get-Content "$root\tools\ci\mt6630-provider1030-signing-pins.json" -Raw|ConvertFrom-Json
 if((Sha $sign) -cne $pins.signtool -or (Sha $inf2cat) -cne $pins.inf2cat){throw 'Actual signing tools required'}
 & $inf2cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT signing failed'};$receipt.CAT_signed_this_attempt=1
 foreach($x in @($sys,$cat)){$s=Get-AuthenticodeSignature $x;if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF validity failed'}}
 foreach($name in @('mt6630-combo-runtime.inf','mt6630-combo-runtime.sys')){& $sign verify /pa /c $cat "$dir\$name";if($LASTEXITCODE){throw 'Actual CAT membership failed'}}
 if((Sha $sys) -cne $i.signed_SYS.sha256){throw 'Retained signed SYS byte change'}
 CheckInputs
 if((& git -C $root rev-parse HEAD).Trim() -cne $commit -or @(& git -C $root status --porcelain).Count){throw 'Public source changed'}
 foreach($x in $i.objects|Where-Object {$_.compiled_in_prior_attempt}){Copy-Item $x.object_path "$out\incremental-objects\";if(Test-Path ($x.object_path+'.d')){Copy-Item ($x.object_path+'.d') "$out\incremental-objects\"}}
 Copy-Item $Inputs "$out\evidence\catalog-resume-inputs.json";Copy-Item $i.prior_receipt.path "$out\evidence\prior-build-receipt.json";Copy-Item $i.plan.path "$out\evidence\compile-plan.json"
 $manifest=@{schema='mt6630-provider1030-incremental-signed-v1';source_commit=$commit;driver_compiled_source_commit=$i.compile_source_commit;role='provider';version='2026.11.11.1030';driver_date='11/10/2026';signer_thumbprint=$thumb;signature_status='Valid';baseline_manifest_sha256=$i.baseline_manifest_sha256;baseline_source_commit='c11981553cc10e9730565390f5d9f9e09a919992';objects=$i.objects;flags=(Get-Content 'C:\mt6630-provider1029\source\build\provider1029\package\provider\signed-manifest.json' -Raw|ConvertFrom-Json).flags;tools=$i.tools;new_compiled_units_in_prior_attempt=$b.compiled;reused_units_in_prior_attempt=$b.reused;compiled_this_attempt=0;linked_this_attempt=0;SYS_byte_retained_after_INF_date_correction=$true;historical_system_include_closure_available=$false;strict_cache_qualified=$false;legacy_checkpoint_build=1030;known_absent_radio_failure_delete_fix=$true;trace_retention_aeacc=$true;hardware_verified=$false;association_verified=$false;ip_communication_verified=$false;SDK_setup=$false;PCO_rebuilt=$false;GPU_rebuilt=$false;parent_rebuilt=$false;files=@(Get-ChildItem $dir -File|ForEach-Object {@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})}
 $manifest|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$dir\signed-manifest.json"
 $receipt.success=$true;$receipt.status='signed-artifact-verified-not-hardware';$receipt.signed_SYS_sha256=Sha $sys;$receipt.signed_manifest_sha256=Sha "$dir\signed-manifest.json"
} catch {$receipt.error=$_.Exception.ToString();throw} finally {$receipt|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$out\signing-receipt.json"}
Compress-Archive "$out\package\*","$out\evidence","$out\incremental-objects","$out\signing-receipt.json" "$out\signed1030.zip"
Write-Output ('SIGNED_MANIFEST_SHA '+$receipt.signed_manifest_sha256)
Write-Output ('SIGNED_ZIP_SHA '+(Sha "$out\signed1030.zip"))
Write-Output 'MT6630_PROVIDER1030_SIGNED_RECOVERABLE_NOT_COMMUNICATION_SUCCESS'
