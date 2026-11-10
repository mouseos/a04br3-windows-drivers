# main専用。既署名EXEを保持しnative単独再開→manifest/archive。compile/link/signなし。
param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$ExpectedSignedExeSha256,[Parameter(Mandatory=$true)][string]$SignTool)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Require($ok,$m){if(!$ok){throw $m}}
$root=[IO.Path]::GetFullPath("$PSScriptRoot\..\..");$out=[IO.Path]::GetFullPath($OutputDirectory);$inputRoot=[IO.Path]::GetFullPath($Inputs).TrimEnd('\')
$binaryCommit='9b3fa5d4546b12ccec6bed6127b3db9900bf47c4';$name='MT8167Usb1121Install198';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
Require (!$out.Equals($root) -and $out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -and (Test-Path $out)) 'existing checkout-local signed output required'
Require (@(& git -C $root status --porcelain).Count -eq 0) 'clean fixed resume source required'
$recipeCommit=(& git -C $root rev-parse HEAD)
Require ($ExpectedSignedExeSha256 -cmatch '^[0-9a-f]{64}$') 'explicit existing signed EXE hash required'
Require ((Sha "$Inputs\input-closure.json") -ceq '09fcdfca60dac35fd3572a23d43c9de4100ab70e799ccdb9e1bd6bb7b2598955') 'fresh198 closure mismatch'
$c=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
foreach($f in $c.files.PSObject.Properties){$p=[IO.Path]::GetFullPath((Join-Path $Inputs $f.Name));Require ($p.StartsWith($inputRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and (Sha $p) -ceq $f.Value) 'input leaf drift'}
Require (!(Test-Path "$out\service-manifest.json") -and !(Test-Path "$out\usb1121-service198-signed.zip")) 'completed output exists; keep it immutable'
$exe="$out\payload\$name.exe";$before=Sha $exe
Require ($before -ceq $ExpectedSignedExeSha256) 'existing signed EXE drift'
$rows=@(Get-ChildItem "$out\payload" -Recurse -File);$expectedPayload=@($c.files.PSObject.Properties|Where-Object{$_.Name.StartsWith('payload/')})
Require ($rows.Count -eq $expectedPayload.Count+1) 'unexpected payload members'
foreach($f in $expectedPayload){Require ((Sha (Join-Path $out $f.Name)) -ceq $f.Value) 'signed payload/input mismatch'}
Require ((Sha $SignTool) -ceq '854c356a9d9977fb2ed22ce04b531ab6f4417da06a4dba6e3d3d628b9cfb988c') 'approved SignTool required'
$sig=Get-AuthenticodeSignature $exe;Require ($sig.Status -eq 'Valid' -and $sig.SignerCertificate.Thumbprint -ceq $thumb) 'existing signed EXE trust mismatch'
& $SignTool verify /pa /all $exe;Require ($LASTEXITCODE -eq 0) 'existing EXE verify failed'
# Source provenance: native fix may change recipes only, never the signed runtime source.
$runtime=@('tools/usb/mt8167-usb1121-install-service.c','tools/usb/run-usb1121-service.ps1')
$sourceProof=@();foreach($n in $runtime){$old=(& git -C $root rev-parse ($binaryCommit+':'+$n));$now=(& git -C $root hash-object (Join-Path $root $n));Require ($LASTEXITCODE -eq 0 -and $old -ceq $now) 'signed runtime source differs from original build';$sourceProof+=@{path=$n;git_blob=$old;sha256=(Sha (Join-Path $root $n))}}
Require ((Sha "$out\usb1121-service-pins.h") -ceq $c.files.'usb1121-service-pins.h') 'original compiled header drift'
$obj="$out\$name.obj";$objBefore=Sha $obj
# Re-verify existing DLL/catalog membership, no re-signing.
$helper="$out\payload\helper\MT8167UsbAbi3Pnp.dll";$hs=Get-AuthenticodeSignature $helper
Require ($hs.Status -eq 'Valid' -and $hs.SignerCertificate.Thumbprint -ceq $thumb) 'helper trust mismatch'
$plan=Get-Content "$Inputs\payload\bundle\plan.json" -Raw|ConvertFrom-Json
foreach($e in @($plan.candidate.parent,$plan.candidate.hcd,$plan.rollback.parent,$plan.rollback.legacy)){
 $cat=Join-Path "$out\payload\bundle" $e.names.cat;$cs=Get-AuthenticodeSignature $cat
 Require ($cs.Status -eq 'Valid' -and $cs.SignerCertificate.Thumbprint -ceq $thumb) 'catalog trust mismatch'
 foreach($n in @($e.names.inf,$e.names.sys)){& $SignTool verify /pa /c $cat (Join-Path "$out\payload\bundle" $n);Require ($LASTEXITCODE -eq 0) 'catalog member mismatch'}
}
# Preserve the failed clone. Do not load it or trust partial mutations as a baseline.
$archive=$null
if(Test-Path "$out\native"){
 $archive='native-failed-'+[Guid]::NewGuid().ToString('N')
 Move-Item -LiteralPath "$out\native" -Destination "$out\$archive"
}
& "$root\tools\recovery\prepare-usb1121-system198.ps1" -Inputs $Inputs -OutputDirectory "$out\native"
Require ($?) 'native-only preparation failed'
Require ((Sha $exe) -ceq $before -and (Sha $obj) -ceq $objBefore) 'existing EXE/object changed during native resume'
$files=[ordered]@{};Get-ChildItem "$out\payload" -Recurse -File|ForEach-Object{$files[$_.FullName.Substring($out.Length+1).Replace('\','/')]=Sha $_.FullName}
$nativeFiles=[ordered]@{};Get-ChildItem "$out\native" -File|ForEach-Object{$nativeFiles[$_.Name]=Sha $_.FullName}
$m=[ordered]@{native_output_hashes=$nativeFiles;schema='MT8167_USB1121_SIGNED_SERVICE198_V1';source_commit=$binaryCommit;recipe_source_commit=$recipeCommit;binary_build_recipe_commit=$binaryCommit;source_sha256=(Sha "$root\tools\usb\mt8167-usb1121-install-service.c");runtime_source_proof=$sourceProof;input_closure_sha256=(Sha "$Inputs\input-closure.json");profile_sha256=$c.profile_sha256;fresh_sha256=$c.fresh_sha256;native_receipt_sha256=$c.native_sha256;service=$name;signature_status='Valid';signer=$thumb;unsigned_exe_sha256=$null;unsigned_exe_hash_not_preserved_before_native_failure=$true;signed_exe_sha256=$before;signed_exe_reused_byte_identical=$true;existing_object_sha256=$objBefore;files=$files;native_result_sha256=(Sha "$out\native\result.json");prepared_SYSTEM_sha256=(Sha "$out\native\SYSTEM");native_helper_sha256=(Sha "$root\tools\recovery\Usb1121OfflineHiveNative.cs");failed_native_directory_preserved=$archive;signtool_sha256=(Sha $SignTool);initial_compile_sign_evidence='main build-service198-host.log(.stderr); initial process precedes native failure';initial_service_compile_TUs=1;service_compile_TUs_this_resume=0;link_invocations_this_resume=0;sign_invocations_this_resume=0;driver_rebuilds=0;helper_rebuilds=0;SDK_setup=$false;hardware_applied=$false;power_verified=$false}
[IO.File]::WriteAllText("$out\service-manifest.json",($m|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)))
Compress-Archive -Path "$out\payload","$out\native","$out\service-manifest.json" -DestinationPath "$out\usb1121-service198-signed.zip"
Require ((Sha $exe) -ceq $before -and (Sha $obj) -ceq $objBefore) 'existing EXE/object changed during packaging'
Get-FileHash "$out\usb1121-service198-signed.zip","$out\service-manifest.json",$exe -Algorithm SHA256
'USB1121_NATIVE_RESUMED_EXISTING_SIGNED_EXE_UNCHANGED_NOT_HARDWARE_SUCCESS'
