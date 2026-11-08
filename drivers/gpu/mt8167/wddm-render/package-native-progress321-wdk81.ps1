param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineDirectory,[Parameter(Mandatory=$true)][string]$KernelDirectory,[Parameter(Mandatory=$true)][string]$ClientFile,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$ExpectedKernelBuildManifestSha256)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit 321 native progress candidate required'}
$dpRoot=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));if(@(& git -C $dpRoot status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Exact input pin mismatch: $p"}}
$dpThumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
Pin "$BaselineDirectory\signed-manifest.json" '86557e13fbac04f3d38b3949516f47b14435fa2e6c3c411e5e2319209dee2669'
Pin "$KernelDirectory\combined-manifest.json" $ExpectedKernelBuildManifestSha256
Pin "$BaselineDirectory\trace-signed-manifest.json" '6bc00603aea90762a04b125667645979c1887d40e4632e0105d44a51e449a9a3'
Pin $ClientFile 'f3af58c14f7a3efdee119dac971d59b5e92c069bd649a6499006242d37698597'
$dpBase=Get-Content "$BaselineDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$dpKernel=Get-Content "$KernelDirectory\combined-manifest.json" -Raw|ConvertFrom-Json
$dpTrace=Get-Content "$BaselineDirectory\trace-signed-manifest.json" -Raw|ConvertFrom-Json
if($dpBase.inf_driverver -cne '11/07/2026,0.0.3.20' -or $dpKernel.package_source_commit -cne (& git -C $dpRoot rev-parse HEAD) -or $dpKernel.kmd_source_commit -cne 'f73da1e31755e08727958ac6f9160a0d5ccdff6b' -or $dpKernel.schema -cne 'native-progress321-combined-v1' -or !$dpKernel.private_dma_cursor_consumed -or $dpKernel.private_pointer_input_preserved -or $dpKernel.private_dma_expected_used_bytes -ne 16 -or !$dpKernel.render_before_residency -or !$dpKernel.patch_passive_prepare -or $dpKernel.native_source_commit -cne 'e0614136f88262f52d60f6458b5e3811de04fdb6' -or $dpKernel.signature_status -cne 'Valid' -or $dpKernel.signer_thumbprint -cne $dpThumb -or $dpTrace.source_commit -cne '3f8291d094c01206dfaa0a22d1fdccd6eecc5823' -or $dpTrace.signature_status -cne 'Valid' -or $dpTrace.signer_thumbprint -cne $dpThumb -or !$dpKernel.instrumentation_only -or $dpKernel.native_units.Count -ne 2){throw 'Independent source/signature schema mismatch'}
foreach($dpE in $dpBase.outputs){Pin (Join-Path $BaselineDirectory $dpE.name) $dpE.sha256}
foreach($dpE in $dpKernel.outputs){Pin (Join-Path $KernelDirectory $dpE.name) $dpE.sha256}
Pin "$BaselineDirectory\ge8300-umd.dll" 'b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe'
Pin "$KernelDirectory\ge8300-render.sys" (@($dpKernel.outputs|Where-Object {$_.name -ceq 'ge8300-render.sys'})[0].sha256)
$dpOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$dpOut.StartsWith($dpRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $dpOut)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $dpOut|Out-Null
foreach($dpN in @('ge8300-mips.fw','mt8167-development.cer','ge8300-umd.dll','native-dependency-closure.json','trace-signed-manifest.json')){Copy-Item "$BaselineDirectory\$dpN" $dpOut}
Copy-Item "$KernelDirectory\ge8300-render.sys" $dpOut
Copy-Item $ClientFile "$dpOut\ge8300-d3d-clear.exe"
Copy-Item "$dpOut\native-dependency-closure.json" "$dpOut\baseline-native-dependency-closure.json"
Copy-Item "$KernelDirectory\native-progress-manifest.json" $dpOut
[ordered]@{schema='native-progress321-incremental-closure-v1';baseline_native_source_commit='fb2173597de9acc12af8f06fd771037e02093360';baseline_dependency_closure_sha256=(Sha "$dpOut\baseline-native-dependency-closure.json");native_source_commit=$dpKernel.native_source_commit;native_progress_manifest_sha256=(Sha "$dpOut\native-progress-manifest.json");changed_units=@('native_owner_factory','native_owner_factory_runtime');changed_abi=$false;runtime_verified=$false}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$dpOut\native-dependency-closure.json"
$dpInf=[IO.File]::ReadAllText("$BaselineDirectory\ge8300-render.inf")
if(([regex]::Matches($dpInf,[regex]::Escape('DriverVer=11/07/2026,0.0.3.20'))).Count -ne 1){throw 'Exact baseline319 INF required'}
[IO.File]::WriteAllText("$dpOut\ge8300-render.inf",$dpInf.Replace('DriverVer=11/07/2026,0.0.3.20','DriverVer=11/07/2026,0.0.3.21'),[Text.Encoding]::Unicode)
Copy-Item "$BaselineDirectory\signed-manifest.json" "$dpOut\baseline-signed-manifest.json"
Copy-Item "$KernelDirectory\combined-manifest.json" "$dpOut\kernel-build-manifest.json"
[ordered]@{schema='linear-kmd-native-signed-image-v1';source_commit=$dpKernel.kmd_source_commit;native_owner_source_commit=$dpKernel.native_source_commit;signature_status='Valid';signer_thumbprint=$dpThumb;build_manifest_sha256=(Sha "$dpOut\kernel-build-manifest.json");outputs=@(@{name='ge8300-render.sys';sha256=(Sha "$dpOut\ge8300-render.sys")});hardware_verified=$false}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$dpOut\kernel-signed-manifest.json"
$dpKit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$dpSign="$dpKit\bin\x86\signtool.exe"
& "$dpKit\bin\x86\Inf2Cat.exe" "/driver:$dpOut" /os:6_3_ARM;if($LASTEXITCODE){throw 'Inf2Cat failed'}
& $dpSign sign /fd SHA256 /sha1 $dpThumb "$dpOut\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($dpN in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $dpSign verify /pa /c "$dpOut\ge8300-render.cat" "$dpOut\$dpN";if($LASTEXITCODE){throw 'CAT membership failed'}}
foreach($dpN in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat','ge8300-d3d-clear.exe')){$dpSig=Get-AuthenticodeSignature "$dpOut\$dpN";if($dpSig.Status -ne 'Valid' -or $dpSig.SignerCertificate.Thumbprint -ne $dpThumb){throw 'Image/CAT signature invalid'}}
Pin "$dpOut\ge8300-render.sys" (Sha "$KernelDirectory\ge8300-render.sys")
Pin "$dpOut\ge8300-umd.dll" 'b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe'
Pin "$dpOut\ge8300-mips.fw" '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
Pin "$dpOut\ge8300-d3d-clear.exe" 'f3af58c14f7a3efdee119dac971d59b5e92c069bd649a6499006242d37698597'
@{schema='native-progress321-independent-sources-v1';package_source_commit=(& git -C $dpRoot rev-parse HEAD);kernel_binary_source_commit=$dpKernel.kmd_source_commit;native_owner_source_commit=$dpKernel.native_source_commit;umd_source_commit=$dpTrace.source_commit;client_source_commit=$dpTrace.client_source_commit;baseline_manifest_sha256=(Sha "$BaselineDirectory\signed-manifest.json");trace_manifest_sha256=(Sha "$BaselineDirectory\trace-signed-manifest.json");kernel_manifest_sha256=(Sha "$dpOut\kernel-signed-manifest.json");kernel_build_manifest_sha256=(Sha "$dpOut\kernel-build-manifest.json");native_dependency_closure_sha256=(Sha "$dpOut\native-dependency-closure.json");inf_driverver='11/07/2026,0.0.3.21';signer_thumbprint=$dpThumb;signature_status='Valid';kernel_changed=$true;native_owner_changed=$true;firmware_changed=$false;umd_changed=$false;client_changed=$false;permanent_sysmem_changed=$false;limited_development_profile=$true;standard_profile_qualified=$false;development_negotiation_pipeline='10_0';allocation_list_capacity=3;patch_location_capacity=2;dma_buffer_size_changed=$false;private_pointer_input_preserved=$false;private_dma_cursor_consumed=$true;private_dma_expected_used_bytes=16;render_before_residency=$true;patch_passive_prepare=$true;owned_payload_relocation=$true;selected_mdl_spans=$true;software_cache_coherent_maps=$true;runtime_verified=$false;outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll','mt8167-development.cer','ge8300-d3d-clear.exe')|ForEach-Object{@{name=$_;sha256=(Sha "$dpOut\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$dpOut\signed-manifest.json"
'NATIVE_PROGRESS321_PACKAGE_SIGNED'
