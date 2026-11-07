param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineDirectory,[Parameter(Mandatory=$true)][string]$KernelDirectory,[Parameter(Mandatory=$true)][string]$TraceDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit linear316 candidate required'}
$lpRoot=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));if(@(& git -C $lpRoot status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Exact input pin mismatch: $p"}}
$lpThumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
Pin "$BaselineDirectory\signed-manifest.json" '7cf05956199de556415b5dad8c8d2bb5f99a43f2c1c39b4ff3e8367989d81590'
Pin "$KernelDirectory\combined-manifest.json" '64e5a57e3e3ce12ad086b3990608fe906ac796e1ce8c7a75333a6df7eb0e83d3'
Pin "$TraceDirectory\signed-manifest.json" '6bc00603aea90762a04b125667645979c1887d40e4632e0105d44a51e449a9a3'
$lpBase=Get-Content "$BaselineDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$lpKernel=Get-Content "$KernelDirectory\combined-manifest.json" -Raw|ConvertFrom-Json
$lpTrace=Get-Content "$TraceDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
if($lpBase.inf_driverver -cne '11/07/2026,0.0.3.15' -or $lpKernel.kmd_source_commit -cne '78ca65653d4c9452b50e0ca455bd9c96a27fe2fb' -or $lpKernel.native_source_commit -cne 'fb2173597de9acc12af8f06fd771037e02093360' -or $lpKernel.signature_status -cne 'Valid' -or $lpKernel.signer_thumbprint -cne $lpThumb -or $lpTrace.source_commit -cne '3f8291d094c01206dfaa0a22d1fdccd6eecc5823' -or $lpTrace.signature_status -cne 'Valid' -or $lpTrace.signer_thumbprint -cne $lpThumb){throw 'Independent source/signature schema mismatch'}
foreach($lpE in $lpBase.outputs){Pin (Join-Path $BaselineDirectory $lpE.name) $lpE.sha256}
foreach($lpE in $lpKernel.outputs){Pin (Join-Path $KernelDirectory $lpE.name) $lpE.sha256}
foreach($lpE in $lpTrace.outputs){Pin (Join-Path $TraceDirectory $lpE.name) $lpE.sha256}
Pin "$TraceDirectory\ge8300-umd.dll" 'b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe'
Pin "$TraceDirectory\ge8300-d3d-clear.exe" 'f3af58c14f7a3efdee119dac971d59b5e92c069bd649a6499006242d37698597'
Pin "$KernelDirectory\ge8300-render.sys" 'd0a65ab0639b32f535eb735cfed90ee5732e750d35149650df935218c6e3ac22'
$lpOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!$lpOut.StartsWith($lpRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $lpOut)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $lpOut|Out-Null
foreach($lpN in @('ge8300-mips.fw','mt8167-development.cer')){Copy-Item "$BaselineDirectory\$lpN" $lpOut}
Copy-Item "$KernelDirectory\ge8300-render.sys" $lpOut;Copy-Item "$TraceDirectory\ge8300-umd.dll" $lpOut
$lpInf=[IO.File]::ReadAllText("$BaselineDirectory\ge8300-render.inf")
if(([regex]::Matches($lpInf,[regex]::Escape('DriverVer=11/07/2026,0.0.3.15'))).Count -ne 1){throw 'Exact baseline315 INF required'}
[IO.File]::WriteAllText("$lpOut\ge8300-render.inf",$lpInf.Replace('DriverVer=11/07/2026,0.0.3.15','DriverVer=11/07/2026,0.0.3.16'),[Text.Encoding]::Unicode)
Copy-Item "$BaselineDirectory\signed-manifest.json" "$lpOut\baseline-signed-manifest.json"
Copy-Item "$KernelDirectory\combined-manifest.json" "$lpOut\kernel-build-manifest.json"
Copy-Item "$KernelDirectory\native-dependency-closure.json" "$lpOut\native-dependency-closure.json"
Copy-Item "$TraceDirectory\signed-manifest.json" "$lpOut\trace-signed-manifest.json"
[ordered]@{schema='linear-kmd-native-signed-image-v1';source_commit=$lpKernel.kmd_source_commit;native_owner_source_commit=$lpKernel.native_source_commit;signature_status='Valid';signer_thumbprint=$lpThumb;build_manifest_sha256=(Sha "$lpOut\kernel-build-manifest.json");outputs=@(@{name='ge8300-render.sys';sha256=(Sha "$lpOut\ge8300-render.sys")});hardware_verified=$false}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$lpOut\kernel-signed-manifest.json"
$lpKit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$lpSign="$lpKit\bin\x86\signtool.exe"
& "$lpKit\bin\x86\Inf2Cat.exe" "/driver:$lpOut" /os:6_3_ARM;if($LASTEXITCODE){throw 'Inf2Cat failed'}
& $lpSign sign /fd SHA256 /sha1 $lpThumb "$lpOut\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($lpN in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $lpSign verify /pa /c "$lpOut\ge8300-render.cat" "$lpOut\$lpN";if($LASTEXITCODE){throw 'CAT membership failed'}}
foreach($lpN in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat')){$lpSig=Get-AuthenticodeSignature "$lpOut\$lpN";if($lpSig.Status -ne 'Valid' -or $lpSig.SignerCertificate.Thumbprint -ne $lpThumb){throw 'Image/CAT signature invalid'}}
Pin "$lpOut\ge8300-render.sys" 'd0a65ab0639b32f535eb735cfed90ee5732e750d35149650df935218c6e3ac22'
Pin "$lpOut\ge8300-mips.fw" '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
@{schema='linear316-independent-sources-v1';package_source_commit=(& git -C $lpRoot rev-parse HEAD);kernel_binary_source_commit=$lpKernel.kmd_source_commit;native_owner_source_commit=$lpKernel.native_source_commit;umd_source_commit=$lpTrace.source_commit;client_source_commit=$lpTrace.client_source_commit;baseline_manifest_sha256=(Sha "$BaselineDirectory\signed-manifest.json");trace_manifest_sha256=(Sha "$TraceDirectory\signed-manifest.json");kernel_manifest_sha256=(Sha "$lpOut\kernel-signed-manifest.json");kernel_build_manifest_sha256=(Sha "$lpOut\kernel-build-manifest.json");native_dependency_closure_sha256=(Sha "$lpOut\native-dependency-closure.json");inf_driverver='11/07/2026,0.0.3.16';signer_thumbprint=$lpThumb;signature_status='Valid';kernel_changed=$true;native_owner_changed=$true;firmware_changed=$false;permanent_sysmem_changed=$false;limited_development_profile=$true;standard_profile_qualified=$false;development_negotiation_pipeline='10_0';runtime_verified=$false;outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll','mt8167-development.cer')|ForEach-Object{@{name=$_;sha256=(Sha "$lpOut\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$lpOut\signed-manifest.json"
'LINEAR316_PACKAGE_SIGNED'
