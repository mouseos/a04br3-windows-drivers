# 322 SYS/FW/client and exact native/KMD provenance retained; new UMD + INF/CAT.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineDirectory,[Parameter(Mandatory=$true)][string]$UmdDirectory,[Parameter(Mandatory=$true)][string]$ExpectedUmdManifestSha256,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit323 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if($h -notmatch '^[a-fA-F0-9]{64}$' -or (Sha $p) -cne $h.ToLowerInvariant()){throw "Exact input mismatch: $p"}}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
Pin "$BaselineDirectory\signed-manifest.json" '59bb556d372d923c9b7b8bfb3ea08b61f9d3f5c18c851f8f9e8750f2c7185a29'
$base=Get-Content "$BaselineDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
if($base.schema -cne 'implemented-reset322-independent-sources-v1' -or $base.inf_driverver -cne '11/07/2026,0.0.3.22' -or $base.kernel_binary_source_commit -cne 'f73da1e31755e08727958ac6f9160a0d5ccdff6b' -or $base.native_owner_source_commit -cne 'f96948533601f9fdcdcc22b9700aacd96dbf7204' -or $base.client_source_commit -cne 'f4f1b4f94ade6f59efef042f680c2c3e363442f8' -or $base.signature_status -cne 'Valid' -or $base.signer_thumbprint -cne $thumb){throw 'Exact322 baseline source/signature required'}
foreach($e in $base.outputs){Pin (Join-Path $BaselineDirectory $e.name) $e.sha256}
Pin "$BaselineDirectory\kernel-signed-manifest.json" $base.kernel_manifest_sha256
Pin "$BaselineDirectory\kernel-build-manifest.json" $base.kernel_build_manifest_sha256
Pin "$BaselineDirectory\native-dependency-closure.json" $base.native_dependency_closure_sha256
Pin "$UmdDirectory\build-manifest.json" $ExpectedUmdManifestSha256
$umd=Get-Content "$UmdDirectory\build-manifest.json" -Raw|ConvertFrom-Json
if($umd.schema -cne 'rt81-shader-signature323-umd-build-v1' -or $umd.source_commit -cne $source -or $umd.shader_delta_source_commit -cne 'eb4dca0ee3134c9a9baea4021325f4a924872444' -or $umd.umd_baseline_source_commit -cne '3f8291d094c01206dfaa0a22d1fdccd6eecc5823' -or $umd.changed_member_count -ne 1 -or $umd.delta_compiled_units.Count -ne 1 -or $umd.delta_compiled_units[0] -cne 'umd/tokenized_shader' -or $umd.compiler_rebuilt -or $umd.pair_rebuilt -or $umd.client_rebuilt -or $umd.kernel_rebuilt -or $umd.native_rebuilt -or $umd.signature_status -cne 'Valid' -or $umd.signer_thumbprint -cne $thumb){throw 'Exact signed UMD delta provenance required'}
foreach($e in $umd.outputs){Pin (Join-Path $UmdDirectory $e.name) $e.sha256}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New source-local package output required'}
New-Item -ItemType Directory $out|Out-Null
# Copy all322 provenance bytes/ancestor closures intact before changing inputs.
Get-ChildItem $BaselineDirectory -File|Where-Object{$_.Name -ne 'signed-manifest.json'}|Copy-Item -Destination $out
Copy-Item "$BaselineDirectory\signed-manifest.json" "$out\retained322-signed-manifest.json"
Copy-Item "$UmdDirectory\build-manifest.json" "$out\umd-signature323-build-manifest.json"
Copy-Item "$UmdDirectory\ge8300-umd.dll" "$out\ge8300-umd.dll" -Force
$inf=[IO.File]::ReadAllText("$out\ge8300-render.inf",[Text.Encoding]::Unicode)
$old='DriverVer=11/07/2026,0.0.3.22';$new='DriverVer=11/07/2026,0.0.3.23'
if(([regex]::Matches($inf,[regex]::Escape($old))).Count -ne 1){throw 'Exact322 INF metadata required'}
[IO.File]::WriteAllText("$out\ge8300-render.inf",$inf.Replace($old,$new),[Text.Encoding]::Unicode)
Remove-Item "$out\ge8300-render.cat"
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign="$kit\bin\x86\signtool.exe"
& "$kit\bin\x86\Inf2Cat.exe" "/driver:$out" /os:6_3_ARM;if($LASTEXITCODE){throw 'Inf2Cat323'}
& $sign sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT323 sign'}
foreach($n in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $sign verify /pa /c "$out\ge8300-render.cat" "$out\$n";if($LASTEXITCODE){throw "CAT323 membership $n"}}
foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat','ge8300-d3d-clear.exe')){$s=Get-AuthenticodeSignature "$out\$n";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw "Signer323 $n"}}
foreach($n in @('ge8300-render.sys','ge8300-mips.fw','ge8300-d3d-clear.exe','mt8167-development.cer')){Pin "$out\$n" (Sha "$BaselineDirectory\$n")}
$metadata=[ordered]@{};foreach($p in $base.PSObject.Properties){$metadata[$p.Name]=$p.Value}
$metadata.schema='rt81-shader-signature323-independent-sources-v1';$metadata.package_source_commit=$source
$metadata.baseline_manifest_sha256=(Sha "$BaselineDirectory\signed-manifest.json")
$metadata.umd_source_commit='eb4dca0ee3134c9a9baea4021325f4a924872444';$metadata.umd_baseline_source_commit='3f8291d094c01206dfaa0a22d1fdccd6eecc5823'
$metadata.umd_delta_build_source_commit=$source;$metadata.umd_build_manifest_sha256=(Sha "$out\umd-signature323-build-manifest.json")
$metadata.baseline_archive_reconstructed=$umd.baseline_archive_reconstructed;$metadata.historical_baseline_archive_byte_equal=$umd.historical_baseline_archive_byte_equal
$metadata.kernel_changed=$false;$metadata.native_owner_changed=$false;$metadata.firmware_changed=$false;$metadata.umd_changed=$true;$metadata.client_changed=$false
$metadata.inf_driverver='11/07/2026,0.0.3.23';$metadata.runtime_verified=$false
$metadata.outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll','mt8167-development.cer','ge8300-d3d-clear.exe')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}
$metadata.retained_provenance=@(Get-ChildItem $BaselineDirectory -File -Filter '*.json'|Where-Object{$_.Name -ne 'signed-manifest.json'}|ForEach-Object{Pin "$out\$($_.Name)" (Sha $_.FullName);@{name=$_.Name;sha256=(Sha $_.FullName)}})
$metadata|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
'RT81_SHADER_SIGNATURE323_PACKAGE_SIGNED'
