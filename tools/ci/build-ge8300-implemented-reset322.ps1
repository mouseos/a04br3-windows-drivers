# Rebuild WindowsStop only for implemented reset controls; retain exact KMD/SEH/UMD/FW/client.
param([Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Input pin mismatch: $p"}}
Pin "$BaselineBuildDirectory\combined-manifest.json" 'ed88305a4cdf85582ccf7ec64f165a9a19fca0bd63c00e5abbc94f2ceea9035f'
Pin "$BaselineBuildDirectory\ge8300-pair-kmd.lib" '3c818dba1d5b567a21ce1e1f594ebfd9fe03b0f26acf22f83978fe92cf29e4ce'
Pin "$BaselinePackageDirectory\signed-manifest.json" '9d37f9274882cc49dc7d1f5ed044b94d1b8c3be4c5dd818a84918bbf0cac4c07'
$native=Get-Content "$NativeDirectory\native-reset-manifest.json" -Raw|ConvertFrom-Json
if($native.schema -cne 'implemented-reset322-incremental-v1' -or $native.source_commit -cne 'f96948533601f9fdcdcc22b9700aacd96dbf7204' -or ($native.instrumentation_only -or !$native.reset_readback_contract_changed) -or (!$native.gpu_completion_semantics_unchanged -or !$native.timeout_values_unchanged) -or $native.changed_abi -or $native.objects.Count -ne 1){throw 'Exact native progress source required'}
Pin "$NativeDirectory\ge8300-native-owner-core.lib" $native.outputs.sha256
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New source-local output required'}
New-Item -ItemType Directory $out|Out-Null
Copy-Item "$BaselineBuildDirectory\ge8300-pair-kmd.lib" $out
Copy-Item "$NativeDirectory\ge8300-native-owner-core.lib" $out
Copy-Item "$NativeDirectory\native-reset-manifest.json" $out
& "$root\drivers\gpu\mt8167\wddm-render\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$out\ge8300-pair-kmd.lib" -NativeLibrary "$out\ge8300-native-owner-core.lib" -OutputFile "$out\ge8300-render.sys" -MapFile "$out\ge8300-render.map" -MetadataFile "$out\linker.json"
if($LASTEXITCODE){throw 'LINK failed'}
$unsigned=Sha "$out\ge8300-render.sys";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
& $sign sign /fd SHA256 /sha1 $thumb "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS sign failed'}
& $sign verify /pa "$out\ge8300-render.sys";if($LASTEXITCODE){throw 'SYS verify failed'}
[ordered]@{schema='implemented-reset322-combined-v1';package_source_commit=(& git -C $root rev-parse HEAD);kmd_source_commit='f73da1e31755e08727958ac6f9160a0d5ccdff6b';native_source_commit=$native.source_commit;baseline_package_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");native_progress_manifest_sha256=(Sha "$out\native-reset-manifest.json");kmd_units=@();native_units=@('windows_stop');native_rebuilt=$true;umd_rebuilt=$false;changed_abi=$false;instrumentation_only=$false;reset_readback_contract_changed=$true;private_pointer_input_preserved=$false;private_dma_cursor_consumed=$true;private_dma_expected_used_bytes=16;render_before_residency=$true;patch_passive_prepare=$true;owned_payload_relocation=$true;selected_mdl_spans=$true;software_cache_coherent_maps=$true;signature_status='Valid';signer_thumbprint=$thumb;unsigned_sys_sha256=$unsigned;hardware_verified=$false;outputs=@('ge8300-render.sys','ge8300-pair-kmd.lib','ge8300-native-owner-core.lib','native-reset-manifest.json','linker.json')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\combined-manifest.json"
'IMPLEMENTED_RESET322_COMBINED_SIGNED'
