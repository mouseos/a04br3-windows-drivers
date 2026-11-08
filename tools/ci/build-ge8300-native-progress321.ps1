# Only native factory/runtime are rebuilt. KMD/SEH/UMD/FW/client remain exact 3.20 inputs.
param([Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Input pin mismatch: $p"}}
Pin "$BaselineBuildDirectory\combined-manifest.json" 'c0267b50da01d2a94de80ae190d0152699eae6980218290c56025d4605d333f1'
Pin "$BaselineBuildDirectory\ge8300-pair-kmd.lib" '3c818dba1d5b567a21ce1e1f594ebfd9fe03b0f26acf22f83978fe92cf29e4ce'
Pin "$BaselinePackageDirectory\signed-manifest.json" '86557e13fbac04f3d38b3949516f47b14435fa2e6c3c411e5e2319209dee2669'
$native=Get-Content "$NativeDirectory\native-progress-manifest.json" -Raw|ConvertFrom-Json
if($native.schema -cne 'native-progress321-incremental-v1' -or $native.source_commit -cne 'e0614136f88262f52d60f6458b5e3811de04fdb6' -or !$native.instrumentation_only -or !$native.timeout_and_completion_semantics_unchanged -or $native.changed_abi -or $native.objects.Count -ne 2){throw 'Exact native progress source required'}
Pin "$NativeDirectory\ge8300-native-owner-core.lib" $native.outputs.sha256
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New source-local output required'}
New-Item -ItemType Directory $out|Out-Null
Copy-Item "$BaselineBuildDirectory\ge8300-pair-kmd.lib" $out
Copy-Item "$NativeDirectory\ge8300-native-owner-core.lib" $out
Copy-Item "$NativeDirectory\native-progress-manifest.json" $out
& "$root\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$out\ge8300-pair-kmd.lib" -NativeLibrary "$out\ge8300-native-owner-core.lib" -OutputFile "$out\ge8300-render.sys" -MapFile "$out\ge8300-render.map" -MetadataFile "$out\linker.json"
if($LASTEXITCODE){throw 'LINK failed'}
$unsigned=Sha "$out\ge8300-render.sys";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
& $sign sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS sign failed'}
& $sign verify /pa "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS verify failed'}
[ordered]@{schema='native-progress321-combined-v1';package_source_commit=(& git -C $root rev-parse HEAD);kmd_source_commit='f73da1e31755e08727958ac6f9160a0d5ccdff6b';native_source_commit=$native.source_commit;baseline_package_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");native_progress_manifest_sha256=(Sha "$out\native-progress-manifest.json");kmd_units=@();native_units=@('native_owner_factory','native_owner_factory_runtime');native_rebuilt=$true;umd_rebuilt=$false;changed_abi=$false;instrumentation_only=$true;private_pointer_input_preserved=$false;private_dma_cursor_consumed=$true;private_dma_expected_used_bytes=16;render_before_residency=$true;patch_passive_prepare=$true;owned_payload_relocation=$true;selected_mdl_spans=$true;software_cache_coherent_maps=$true;signature_status='Valid';signer_thumbprint=$thumb;unsigned_sys_sha256=$unsigned;hardware_verified=$false;outputs=@('ge8300-render.sys','ge8300-pair-kmd.lib','ge8300-native-owner-core.lib','native-progress-manifest.json','linker.json')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\combined-manifest.json"
'NATIVE_PROGRESS321_COMBINED_SIGNED'
