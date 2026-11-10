param(
 [Parameter(Mandatory=$true)][string]$ObjectPlan,
 [Parameter(Mandatory=$true)][string]$ObjectPlanSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
$prior='C:\mt6630-provider1029\source\build\provider1029';$cache='C:\mt6630-start2015-cache-ae2e0708\closure'
$baselinePath="$prior\package\provider\signed-manifest.json";$baselineHash='d974239fbcf96d7baad96b84fccaa7462a7481058a7c2d80c0d19d4745ef1599'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Test-Path $out) -or !$out.StartsWith($root+'\build\',[StringComparison]::OrdinalIgnoreCase)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean published source required'}
if((Sha $baselinePath) -cne $baselineHash -or (Sha $ObjectPlan) -cne $ObjectPlanSha256){throw 'Actual1029/plan SHA mismatch'}
$baseline=Get-Content $baselinePath -Raw|ConvertFrom-Json;$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
if($plan.source_commit -cne $commit -or $plan.expected_compile_count -ne 3 -or $plan.objects.Count -ne 43){throw 'Fixed1030 source/3TU/43objects required'}
$tools=@();foreach($name in @('clang','lld-link','llvm-readobj')){
 $p=(Get-Command $name -ErrorAction Stop).Source;$old=@($baseline.tools|Where-Object {$_.name -ceq $name})
 if($old.Count -ne 1 -or (Sha $p) -cne $old[0].sha256){throw 'Actual1029 compiler/linker tool mismatch'}
 $tools+=@{name=$name;path=$p;sha256=(Sha $p)}
}
foreach($f in $baseline.files){if((Sha (Join-Path "$prior\package\provider" $f.path)) -cne $f.sha256){throw 'Actual1029 package changed'}}
$cacheManifest=Get-Content "$cache\cache-manifest.json" -Raw|ConvertFrom-Json
if($cacheManifest.source_commit -cne 'ae2e070851fdfb933416897321aec552d84e74a6'){throw 'Actual cache identity required'}
$km="$cache\small\wdk\Lib\winv6.3\km\arm";$libs=@();$libPins=@()
foreach($name in @('ndis','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')){
 $p="$km\$name.lib";$rel='small/wdk/Lib/winv6.3/km/arm/'+$name+'.lib'
 $row=@($cacheManifest.files|Where-Object {$_.path.Replace('\','/') -ceq $rel})
 if($row.Count -ne 1 -or (Sha $p) -cne $row[0].sha256){throw ('Cache library pin mismatch '+$name)}
 $libs+=@($p);$libPins+=@{path=$p;sha256=(Sha $p)}
}
New-Item -ItemType Directory "$out\obj\provider","$out\package\provider","$out\tmp"|Out-Null
$oldTmp=$env:TMP;$oldTemp=$env:TEMP;$env:TMP="$out\tmp";$env:TEMP=$env:TMP
$receipt=@{schema='mt6630-provider1030-build-receipt-v1';source_commit=$commit;baseline_manifest_sha256=$baselineHash;plan_sha256=$ObjectPlanSha256;status='started';compiled=@();reused=@();tools=$tools;kernel_libraries=$libPins;SDK_setup=$false;PCO_rebuilt=$false;GPU_rebuilt=$false;parent_rebuilt=$false;UMD_rebuilt=$false;device_touched=$false;hardware_verified=$false;historical_system_include_closure_available=$false;error=$null}
function SaveReceipt{$receipt|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$out\build-receipt.json"}
try{
 SaveReceipt;$objects=@();$records=@();$seen=@{};$flags=@($baseline.flags)
 foreach($x in $plan.objects){
  if($seen.ContainsKey($x.source_file)) {throw 'Duplicate TU'};$seen[$x.source_file]=$true
  $old=@($baseline.objects|Where-Object {$_.source -ceq $x.source_file});if($old.Count -ne 1 -or $old[0].sha256 -cne $x.cached_sha256){throw 'Actual1029 object mismatch'}
  $changed=$false
  foreach($f in $x.source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Current source closure mismatch'};if($old[0].source_closure.($f.Name) -cne $f.Value){$changed=$true}}
  foreach($f in $old[0].source_closure.PSObject.Properties){if($x.source_closure.($f.Name) -cne $f.Value){$changed=$true}}
  if($changed -ne $x.compile){throw 'TU compile decision mismatch'}
  $input=[IO.Path]::GetFullPath($x.cached_source_path)
  if((!$input.StartsWith($prior+'\',[StringComparison]::OrdinalIgnoreCase) -and !$input.StartsWith($cache+'\',[StringComparison]::OrdinalIgnoreCase)) -or (Sha $input) -cne $x.cached_sha256){throw 'Pinned readonly object input mismatch'}
  $obj=Join-Path "$out\obj" $x.object;$dep=$null;$origin=$old[0].compiled_source_commit
  if($x.compile){
   $dep=$obj+'.d';& clang @flags -MMD -MF $dep -c (Join-Path $root $x.source_file) -o $obj
   if($LASTEXITCODE){throw ('Compile failed '+$x.source_file)}
   $receipt.compiled+=@($x.source_file);$origin=$commit
  }else{Copy-Item -LiteralPath $input -Destination $obj;if((Sha $obj) -cne $x.cached_sha256){throw 'Reused object changed'};$receipt.reused+=@($x.source_file)}
  $records+=@{source=$x.source_file;object=$x.object;compiled=$x.compile;compiled_source_commit=$origin;baseline_manifest_source_commit=$baseline.source_commit;source_closure_matches_current=$true;source_closure=$x.source_closure;sha256=(Sha $obj);dependency_file=$dep;reused_from=$input;reused_input_sha256=$x.cached_sha256};$objects+=@($obj);SaveReceipt
 }
 if($receipt.compiled.Count -ne 3 -or $receipt.reused.Count -ne 40){throw 'Only three changed TUs may compile'}
 $dir="$out\package\provider";$sys="$dir\mt6630-combo-runtime.sys"
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @objects @libs "/out:$sys" "/map:$out\provider.map"
 if($LASTEXITCODE){throw 'Provider link failed'};$receipt.link_count=1
 & llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\provider-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
 $before=& llvm-readobj --coff-imports "$prior\package\provider\mt6630-combo-runtime.sys";if($LASTEXITCODE){throw 'Baseline import read failed'}
 $after=& llvm-readobj --coff-imports $sys;if($LASTEXITCODE){throw 'Candidate import read failed'}
 $before=@($before|Where-Object {$_ -cmatch '^\s*(Name|Symbol):'});$after=@($after|Where-Object {$_ -cmatch '^\s*(Name|Symbol):'})
 if(!$before.Count -or ($before -join "`n") -cne ($after -join "`n")){throw 'Actual1029 imports must remain identical'};$receipt.imports_equal_actual1029=$true
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" $dir
 if(!(Select-String -LiteralPath "$dir\mt6630-combo-runtime.inf" -SimpleMatch 'DriverVer=11/10/2026,2026.11.11.1030')){throw 'Formal1030 INF required'}
 $unsigned=Sha $sys;$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$inf2cat="$cache\small\signing\bin\Inf2Cat.exe";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 $pins1030=Get-Content "$root\tools\ci\mt6630-provider1030-signing-pins.json" -Raw|ConvertFrom-Json
 if((Sha $sign) -cne $pins1030.signtool -or (Sha $inf2cat) -cne $pins1030.inf2cat){throw 'Signing tool pin mismatch'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS signing failed'}
 & $inf2cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My "$dir\mt6630-combo-runtime.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($e in @('sys','cat')){$sig=Get-AuthenticodeSignature "$dir\mt6630-combo-runtime.$e";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF signature verification failed'}}
 foreach($e in @('sys','inf')){& $sign verify /pa /c "$dir\mt6630-combo-runtime.cat" "$dir\mt6630-combo-runtime.$e";if($LASTEXITCODE){throw 'CAT membership failed'}}
 foreach($x in $plan.objects){if((Sha $x.cached_source_path) -cne $x.cached_sha256){throw 'Readonly object input changed'}}
 foreach($x in $libPins){if((Sha $x.path) -cne $x.sha256){throw 'Readonly library changed'}}
 if((Sha $baselinePath) -cne $baselineHash -or (& git -C $root rev-parse HEAD).Trim() -cne $commit -or @(& git -C $root status --porcelain).Count){throw 'Baseline/source changed during build'}
 @{schema='mt6630-provider1030-incremental-v1';source_commit=$commit;role='provider';version='2026.11.11.1030';baseline_manifest_sha256=$baselineHash;baseline_source_commit=$baseline.source_commit;objects=$records;flags=$baseline.flags;tools=$tools;signer_thumbprint=$thumb;signature_status='Valid';unsigned_sha256=$unsigned;legacy_checkpoint_build=1030;strict_cache_qualified=$false;historical_system_include_closure_available=$false;new_compiled_units=$receipt.compiled;reused_units=$receipt.reused;SDK_setup=$false;PCO_rebuilt=$false;GPU_rebuilt=$false;parent_rebuilt=$false;hardware_verified=$false;association_verified=$false;ip_communication_verified=$false;known_absent_radio_failure_delete_fix=$true;trace_retention_aeacc=$true;files=@(Get-ChildItem $dir -File|ForEach-Object {@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$dir\signed-manifest.json"
 $records|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$out\object-manifest.json"
 $receipt.status='signed-artifact-verified-not-hardware';$receipt.unsigned_SYS_sha256=$unsigned;$receipt.signed_SYS_sha256=Sha $sys;SaveReceipt
 Compress-Archive "$out\package\*","$out\build-receipt.json","$out\object-manifest.json","$out\provider-pe.txt" "$out\signed-packages.zip"
 Write-Output 'MT6630_PROVIDER1030_SIGNED_NOT_COMMUNICATION_SUCCESS'
}catch{$receipt.status='failed';$receipt.error=$_.Exception.ToString();SaveReceipt;throw}finally{$env:TMP=$oldTmp;$env:TEMP=$oldTemp}
