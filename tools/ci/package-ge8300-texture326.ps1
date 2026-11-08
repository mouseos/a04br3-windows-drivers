param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$BuildDirectory,[Parameter(Mandatory=$true)][string]$Inf2CatPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit texture candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Pin mismatch $p"}}
Pin "$BaselinePackageDirectory\signed-manifest.json" '8e595170ddec23335e9fb2f7031f45e9eba5f393adae32df01c10b93cc896ca0'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
$b=Get-Content "$BuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
if($b.schema -cne 'texture326-dependency-closure-build-v1' -or $b.source_commit -cne $source -or $b.driver_wire_version -ne 3 -or $b.adapter_contract_version -ne 3 -or $b.SDK_rebuilt -or $b.PCO_rebuilt -or $b.client_rebuilt){throw 'Exact texture build closure required'}
foreach($e in $base.outputs){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
foreach($e in $base.retained_provenance){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
foreach($e in $b.outputs){Pin "$BuildDirectory\$($e.name)" $e.sha256}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local package required'}
New-Item -ItemType Directory $out|Out-Null
# Preserve the baseline provenance graph under the same relative filenames.
# New manifests have distinct names; old closure records are not overwritten.
$retained=@();foreach($f in Get-ChildItem $BaselinePackageDirectory -File){
 if($f.Name -in @('ge8300-render.inf','ge8300-render.cat','ge8300-render.sys','ge8300-umd.dll')){continue}
 $name=if($f.Name -ceq 'signed-manifest.json'){'baseline325-signed-manifest.json'}else{$f.Name}
 Copy-Item $f.FullName "$out\$name";$retained+=@{name=$name;sha256=(Sha "$out\$name")}
}
Copy-Item "$BuildDirectory\build-manifest.json" "$out\texture326-build-manifest.json"
foreach($n in @('ge8300-render.sys','ge8300-umd.dll')){Copy-Item "$BuildDirectory\$n" $out}
$inf=Get-Content "$BaselinePackageDirectory\ge8300-render.inf" -Raw;$old='DriverVer=11/07/2026,0.0.3.25';$new='DriverVer=11/07/2026,0.0.3.26'
if(([regex]::Matches($inf,[regex]::Escape($old))).Count -ne 1){throw 'Exact325 INF required'}
[IO.File]::WriteAllText("$out\ge8300-render.inf",$inf.Replace($old,$new),[Text.Encoding]::Unicode)
& $Inf2CatPath "/driver:$out" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
& $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT sign'}
foreach($n in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $SignToolPath verify /pa /c "$out\ge8300-render.cat" "$out\$n";if($LASTEXITCODE){throw "CAT member $n"}}
foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat','ge8300-d3d-clear.exe')){$s=Get-AuthenticodeSignature "$out\$n";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw "Signer $n"}}
foreach($n in @('ge8300-mips.fw','ge8300-d3d-clear.exe','mt8167-development.cer')){Pin "$out\$n" (Sha "$BaselinePackageDirectory\$n")}
[ordered]@{schema='texture326-native-closure-v1';source_commit=$source;baseline325_signed_manifest_file='baseline325-signed-manifest.json';baseline325_signed_manifest_sha256=(Sha "$out\baseline325-signed-manifest.json");build_manifest_file='texture326-build-manifest.json';build_manifest_sha256=(Sha "$out\texture326-build-manifest.json");native_archive=@($b.archives|Where-Object{$_.role -ceq 'native'});kernel_archive=@($b.archives|Where-Object{$_.role -ceq 'kmd'});native_resolve_ABI_version=1;driver_wire_version=3}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\texture326-native-dependency-closure.json"
$m=@{};foreach($e in $base.PSObject.Properties){$m[$e.Name]=$e.Value}
$m.schema='texture326-independent-sources-v1';$m.package_source_commit=$source;$m.kernel_binary_source_commit=$source;$m.native_owner_source_commit=$source;$m.umd_source_commit=$source
$m.kernel_object_baseline_source_commit=$base.kernel_binary_source_commit;$m.native_owner_baseline_source_commit=$base.native_owner_source_commit;$m.umd_object_baseline_source_commit=$base.umd_source_commit
$m.baseline_manifest_sha256=(Sha "$out\baseline325-signed-manifest.json");$m.baseline_manifest_file='baseline325-signed-manifest.json'
$m.inf_driverver='11/07/2026,0.0.3.26';$m.build_manifest_file='texture326-build-manifest.json';$m.build_manifest_sha256=(Sha "$out\texture326-build-manifest.json");$m.kernel_build_manifest_sha256=$m.build_manifest_sha256;$m.umd_build_manifest_sha256=$m.build_manifest_sha256
$m.native_dependency_closure_file='texture326-native-dependency-closure.json';$m.native_dependency_closure_sha256=(Sha "$out\texture326-native-dependency-closure.json")
$m.kernel_changed=$true;$m.kmd_objects_changed=$true;$m.native_owner_changed=$true;$m.umd_changed=$true;$m.compiler_adapter_changed=$true;$m.compiler_changed=$false;$m.firmware_changed=$false;$m.client_changed=$false
$m.driver_wire_version=3;$m.texture_storage_version=2;$m.adapter_contract_version=3;$m.native_resolve_ABI_version=1;$m.texture_profile='BGRA8-128x128-1mip-POINT-CLAMP-CB0xy-t0-s0';$m.changed_wire_abi=$true;$m.changed_caps=$false;$m.standard_profile_qualified=$false;$m.runtime_verified=$false
$m.compiled_units=$b.compiled_units;$m.archives=$b.archives;$m.retained_provenance=$retained
$m.outputs=@($base.outputs|ForEach-Object{@{name=$_.name;sha256=(Sha "$out\$($_.name)")}})
$m|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
