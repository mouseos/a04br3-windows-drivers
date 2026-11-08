# Independent324 candidate: compiler lane delta only, no driver/client rebuild.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$BuildDirectory,[Parameter(Mandatory=$true)][string]$ExpectedBuildSourceCommit,[Parameter(Mandatory=$true)][string]$Inf2CatPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit independent324 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if($h -notmatch '^[a-fA-F0-9]{64}$' -or (Sha $p) -cne $h.ToLowerInvariant()){throw "Exact pin mismatch: $p"}}
Pin "$BaselinePackageDirectory\signed-manifest.json" 'fd1c90d35fde04663a17285cbb69e91b3545ddf54af1e030a77aacd3813626bf'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$b=Get-Content "$BuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
if($b.schema -cne 'sm4-lane324-compiler-delta-v1' -or $b.source_commit -cne $ExpectedBuildSourceCommit -or $b.functional_source_commit -cne '1b4e06958d00f703fa3649648a2c2e7b18f92a0a' -or $b.changed_member_count -ne 1 -or $b.retained_other_members -ne 32 -or !$b.umd_archive_byte_retained -or $b.pco_full_rebuilt -or $b.hardware_verified){throw 'Exact one-member324 build required'}
if($ExpectedBuildSourceCommit -notmatch '^[a-f0-9]{40}$'){throw 'Exact build source required'}
Pin "$root\drivers\gpu\mt8167\wddm-render\compiler\sm4.c" $b.source_sha256
Pin "$BaselinePackageDirectory\umd-signature323-build-manifest.json" $base.umd_build_manifest_sha256
Pin "$BaselinePackageDirectory\retained322-signed-manifest.json" $base.baseline_manifest_sha256
foreach($e in $base.outputs){Pin (Join-Path $BaselinePackageDirectory $e.name) $e.sha256}
foreach($e in $base.retained_provenance){Pin (Join-Path $BaselinePackageDirectory $e.name) $e.sha256}
foreach($e in $b.outputs){Pin (Join-Path $BuildDirectory $e.name) $e.sha256}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local package required'}
New-Item -ItemType Directory $out|Out-Null
foreach($e in $base.outputs){if($e.name -notin @('ge8300-umd.dll','ge8300-render.cat','ge8300-render.inf')){Copy-Item (Join-Path $BaselinePackageDirectory $e.name) $out}}
foreach($e in $base.retained_provenance){Copy-Item (Join-Path $BaselinePackageDirectory $e.name) $out}
Copy-Item "$BaselinePackageDirectory\signed-manifest.json" "$out\baseline323-signed-manifest.json"
Copy-Item "$BaselinePackageDirectory\retained322-signed-manifest.json" $out
Copy-Item "$BaselinePackageDirectory\umd-signature323-build-manifest.json" "$out\baseline323-umd-build-manifest.json"
Copy-Item "$BuildDirectory\build-manifest.json" "$out\umd-build-manifest.json"
Copy-Item "$BuildDirectory\ge8300-umd.dll" $out
$inf=Get-Content "$BaselinePackageDirectory\ge8300-render.inf" -Raw
$old='DriverVer=11/07/2026,0.0.3.23';$new='DriverVer=11/07/2026,0.0.3.24'
if(([regex]::Matches($inf,[regex]::Escape($old))).Count -ne 1){throw 'Unique exact323 version required'}
[IO.File]::WriteAllText("$out\ge8300-render.inf",$inf.Replace($old,$new),[Text.Encoding]::Unicode)
& $Inf2CatPath "/driver:$out" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE){throw 'Inf2Cat324 failed; do not change date/clock'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
& $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT324 sign'}
& $SignToolPath verify /pa "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT324 verify'}
foreach($n in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){
 & $SignToolPath verify /pa /c "$out\ge8300-render.cat" "$out\$n";if($LASTEXITCODE){throw "CAT membership: $n"}
}
foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-render.cat')){
 $s=Get-AuthenticodeSignature "$out\$n"
 if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw "Exact signer/status: $n"}
}
foreach($e in $base.outputs){if($e.name -notin @('ge8300-umd.dll','ge8300-render.cat','ge8300-render.inf')){Pin "$out\$($e.name)" $e.sha256}}
foreach($e in $base.retained_provenance){Pin "$out\$($e.name)" $e.sha256}
# Inherit existing closure semantics; record compiler and UMD-object origins separately.
$m=@{};foreach($p in $base.PSObject.Properties){$m[$p.Name]=$p.Value}
$m.schema='sm4-lane324-independent-sources-v1';$m.package_source_commit=$source
$m.baseline_manifest_sha256=Sha "$BaselinePackageDirectory\signed-manifest.json"
$m.umd_object_source_commit=$base.umd_source_commit
$m.umd_baseline_source_commit=$base.umd_baseline_source_commit
$m.compiler_delta_source_commit=$b.functional_source_commit
$m.umd_source_commit=$source;$m.umd_delta_build_source_commit=$b.source_commit
$m.umd_build_manifest_sha256=Sha "$out\umd-build-manifest.json"
$m.compiler_baseline_manifest_sha256=$b.compiler_baseline_manifest_sha256
$m.compiler_baseline_archive_sha256=$b.compiler_baseline_archive_sha256
$m.compiler_baseline_manifest_source_commit=$b.compiler_baseline_manifest_source_commit
$m.inf_driverver='11/07/2026,0.0.3.24'
$m.kernel_changed=$false;$m.native_owner_changed=$false;$m.firmware_changed=$false;$m.client_changed=$false;$m.umd_changed=$true
$m.pco_full_rebuilt=$false;$m.nir_util_support_crt_pair_rebuilt=$false;$m.umd_archive_rebuilt=$false
$m.compiler_changed_members=@('sm4_adapter.obj');$m.compiler_retained_members=32
$m.runtime_verified=$false;$m.incorporated_into_run119=$false
$m.outputs=@($base.outputs|ForEach-Object{@{name=$_.name;sha256=(Sha "$out\$($_.name)")}})
$m.retained_provenance=@($base.retained_provenance)+@(@{name='retained322-signed-manifest.json';sha256=(Sha "$out\retained322-signed-manifest.json")},@{name='baseline323-signed-manifest.json';sha256=(Sha "$out\baseline323-signed-manifest.json")},@{name='baseline323-umd-build-manifest.json';sha256=(Sha "$out\baseline323-umd-build-manifest.json")})
$m|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
