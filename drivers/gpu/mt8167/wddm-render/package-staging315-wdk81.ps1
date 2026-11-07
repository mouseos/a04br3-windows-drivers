param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineDirectory,[Parameter(Mandatory=$true)][string]$KernelDirectory,[Parameter(Mandatory=$true)][string]$TraceDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit candidate required'}
$psRoot=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if(@(& git -C $psRoot status --porcelain).Count){throw 'Clean source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pinned($p,$h){if((Sha $p) -cne $h){throw "Hash mismatch: $p"}}
$psThumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
Pinned "$BaselineDirectory\signed-manifest.json" '9ef5d9bfe307f977902b2233bea83c28f92a2971110b4efed1fa1492a55eaab7'
Pinned "$TraceDirectory\signed-manifest.json" 'a2f5e7c2a40849cf6a544daa0499e56b5f49da9f3622a221753a202e714df352'
$psBase=Get-Content "$BaselineDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$psKmd=Get-Content "$KernelDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$psTrace=Get-Content "$TraceDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
if($psKmd.source_commit -cne 'b27164a1738fde39bae51eb32ebcde314216f210' -or $psKmd.permanent_sysmem_changed -or $psTrace.source_commit -cne 'd653e495a5a26e0d3e642c7b3df4aa69074bfe64'){throw 'Source contract mismatch'}
foreach($e in $psBase.outputs){Pinned (Join-Path $BaselineDirectory $e.name) $e.sha256}
foreach($e in $psKmd.outputs){Pinned (Join-Path $KernelDirectory $e.path) $e.sha256}
foreach($e in $psTrace.outputs){Pinned (Join-Path $TraceDirectory $e.name) $e.sha256}
Pinned "$KernelDirectory\ge8300-render.sys" '4f7ad2a5f9158f54f5534930381146663dd3435102c3e6de780a253fdf6778aa'
Pinned "$TraceDirectory\ge8300-umd.dll" '5cf16ccddcdf91796f29bba609c74202f4c79792692cb8054fb6818ef572e22e'
Pinned "$TraceDirectory\ge8300-d3d-clear.exe" 'f3af58c14f7a3efdee119dac971d59b5e92c069bd649a6499006242d37698597'
$psOut=[IO.Path]::GetFullPath($OutputDirectory)
if(!( $psOut.StartsWith($psRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) -or (Test-Path $psOut)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $psOut|Out-Null
foreach($n in @('ge8300-mips.fw','mt8167-development.cer')){Copy-Item "$BaselineDirectory\$n" $psOut}
Copy-Item "$KernelDirectory\ge8300-render.sys" $psOut;Copy-Item "$TraceDirectory\ge8300-umd.dll" $psOut
$psInf=[IO.File]::ReadAllText("$BaselineDirectory\ge8300-render.inf")
if(($psInf -split 'DriverVer=11/07/2026,0.0.3.14').Count -ne 2){throw 'Exact baseline INF required'}
[IO.File]::WriteAllText("$psOut\ge8300-render.inf",$psInf.Replace('DriverVer=11/07/2026,0.0.3.14','DriverVer=11/07/2026,0.0.3.15'),[Text.Encoding]::Unicode)
Copy-Item "$BaselineDirectory\signed-manifest.json" "$psOut\baseline-signed-manifest.json"
Copy-Item "$KernelDirectory\signed-manifest.json" "$psOut\kernel-signed-manifest.json"
Copy-Item "$KernelDirectory\build-manifest.json" "$psOut\kernel-build-manifest.json"
Copy-Item "$TraceDirectory\signed-manifest.json" "$psOut\trace-signed-manifest.json"
$psKit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$psSign="$psKit\bin\x86\signtool.exe"
& "$psKit\bin\x86\Inf2Cat.exe" "/driver:$psOut" /os:6_3_ARM
if($LASTEXITCODE){throw 'Inf2Cat failed'}
& $psSign sign /fd SHA256 /sha1 $psThumb "$psOut\ge8300-render.cat"
if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($n in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $psSign verify /pa /c "$psOut\ge8300-render.cat" "$psOut\$n";if($LASTEXITCODE){throw 'CAT membership failed'}}
foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat')){$psSig=Get-AuthenticodeSignature "$psOut\$n";if($psSig.Status -ne 'Valid' -or $psSig.SignerCertificate.Thumbprint -ne $psThumb){throw 'Signature invalid'}}
@{schema='staging315-independent-sources-v1';package_source_commit=(& git -C $psRoot rev-parse HEAD);kernel_binary_source_commit=$psKmd.source_commit;umd_source_commit=$psTrace.source_commit;client_source_commit=$psTrace.client_source_commit;baseline_manifest_sha256=(Sha "$BaselineDirectory\signed-manifest.json");kernel_manifest_sha256=(Sha "$KernelDirectory\signed-manifest.json");kernel_build_manifest_sha256=(Sha "$KernelDirectory\build-manifest.json");trace_manifest_sha256=(Sha "$TraceDirectory\signed-manifest.json");inf_driverver='11/07/2026,0.0.3.15';signer_thumbprint=$psThumb;signature_status='Valid';kernel_changed=$true;firmware_changed=$false;permanent_sysmem_changed=$false;limited_development_profile=$true;standard_profile_qualified=$false;development_negotiation_pipeline='10_0';runtime_verified=$false;outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll','mt8167-development.cer')|ForEach-Object{@{name=$_;sha256=(Sha "$psOut\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$psOut\signed-manifest.json"
'KMD_TRACE315_PACKAGE_SIGNED'
