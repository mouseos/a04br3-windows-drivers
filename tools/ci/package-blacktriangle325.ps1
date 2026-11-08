param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$BuildDirectory,[Parameter(Mandatory=$true)][string]$Inf2CatPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit325 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Pin mismatch: $p"}}
Pin "$BaselinePackageDirectory\signed-manifest.json" 'fd1c90d35fde04663a17285cbb69e91b3545ddf54af1e030a77aacd3813626bf'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json;$b=Get-Content "$BuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
if($b.source_commit -cne $source -or $b.schema -cne 'blacktriangle325-two-tu-build-v1' -or $b.changed_wire_abi -or $b.compiler_rebuilt){throw 'Fixed325 two-TU build required'}
foreach($e in $base.outputs){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
foreach($e in $base.retained_provenance){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
foreach($e in $b.outputs){Pin "$BuildDirectory\$($e.name)" $e.sha256}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out|Out-Null
foreach($e in $base.outputs){if($e.name -notin @('ge8300-render.inf','ge8300-render.cat','ge8300-render.sys','ge8300-umd.dll')){Copy-Item "$BaselinePackageDirectory\$($e.name)" $out}}
$retained=@();foreach($e in $base.retained_provenance){$name=$e.name;if($name -ceq 'native-dependency-closure.json'){$name='baseline322-native-dependency-closure.json'};Copy-Item "$BaselinePackageDirectory\$($e.name)" "$out\$name";$retained+=@{name=$name;sha256=$e.sha256}}
foreach($leaf in @('retained322-signed-manifest.json','umd-signature323-build-manifest.json')){Copy-Item "$BaselinePackageDirectory\$leaf" $out;$retained+=@{name=$leaf;sha256=(Sha "$out\$leaf")}}
Copy-Item "$BaselinePackageDirectory\signed-manifest.json" "$out\baseline323-signed-manifest.json";$retained+=@{name='baseline323-signed-manifest.json';sha256=(Sha "$out\baseline323-signed-manifest.json")}
Copy-Item "$BuildDirectory\build-manifest.json" "$out\viewport325-build-manifest.json"
foreach($n in @('ge8300-render.sys','ge8300-umd.dll')){Copy-Item "$BuildDirectory\$n" $out}
$inf=Get-Content "$BaselinePackageDirectory\ge8300-render.inf" -Raw;$old='DriverVer=11/07/2026,0.0.3.23';$new='DriverVer=11/07/2026,0.0.3.25'
if(([regex]::Matches($inf,[regex]::Escape($old))).Count -ne 1){throw 'Exact323 INF required'}
[IO.File]::WriteAllText("$out\ge8300-render.inf",$inf.Replace($old,$new),[Text.Encoding]::Unicode)
& $Inf2CatPath "/driver:$out" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat325 failed; do not alter date/clock'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
& $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT sign'}
& $SignToolPath verify /pa "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT verify'}
foreach($n in @('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){& $SignToolPath verify /pa /c "$out\ge8300-render.cat" "$out\$n";if($LASTEXITCODE){throw "CAT membership $n"}}
foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat','ge8300-d3d-clear.exe')){$s=Get-AuthenticodeSignature "$out\$n";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw "Signer/status $n"}}
foreach($n in @('ge8300-mips.fw','ge8300-d3d-clear.exe','mt8167-development.cer')){Pin "$out\$n" (Sha "$BaselinePackageDirectory\$n")}
[ordered]@{schema='viewport325-native-closure-v1';source_commit=$source;baseline_native_owner_source_commit=$base.native_owner_source_commit;baseline_dependency_closure_sha256=$base.native_dependency_closure_sha256;baseline_dependency_closure_file='baseline322-native-dependency-closure.json';native_build_manifest_sha256=(Sha "$out\viewport325-build-manifest.json");baseline_archive_sha256=$b.baseline_native_library_sha256;changed_members=@($b.native_archive_members|Where-Object{$_.changed});retained_members=@($b.native_archive_members|Where-Object{!$_.changed});changed_wire_abi=$false}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\native-dependency-closure.json"
$m=@{};foreach($e in $base.PSObject.Properties){$m[$e.Name]=$e.Value}
$m.schema='blacktriangle325-independent-sources-v1';$m.package_source_commit=$source;$m.baseline_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json")
$m.native_owner_baseline_source_commit=$base.native_owner_source_commit;$m.native_owner_source_commit=$source
$m.umd_object_baseline_source_commit=$base.umd_source_commit;$m.umd_baseline_build_manifest_sha256=$base.umd_build_manifest_sha256;$m.umd_delta_build_source_commit=$source;$m.umd_source_commit=$source
$m.build_manifest_file='viewport325-build-manifest.json';$m.build_manifest_sha256=(Sha "$out\viewport325-build-manifest.json");$m.kernel_build_manifest_sha256=$m.build_manifest_sha256;$m.umd_build_manifest_sha256=$m.build_manifest_sha256
$m.native_dependency_closure_sha256=(Sha "$out\native-dependency-closure.json")
$m.inf_driverver='11/07/2026,0.0.3.25';$m.kernel_changed=$true;$m.kmd_objects_changed=$false;$m.native_owner_changed=$true;$m.umd_changed=$true;$m.compiler_changed=$false;$m.firmware_changed=$false;$m.client_changed=$false;$m.runtime_verified=$false
$m.viewport_ppp_order='Xoffset,Xscale,Yoffset,Yscale,Zoffset,Zscale';$m.native_compiled_units=$b.native_compiled_units;$m.umd_compiled_units=$b.umd_compiled_units;$m.changed_wire_abi=$false;$m.changed_caps=$false
$m.retained_provenance=$retained;$m.outputs=@($base.outputs|ForEach-Object{@{name=$_.name;sha256=(Sha "$out\$($_.name)")}})
$m|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
