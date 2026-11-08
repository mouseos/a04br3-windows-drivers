# Recompile only PCO's sm4_adapter member; retain323 UMD objects/NIR/CRT/pair.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$DependencyDirectory,[Parameter(Mandatory=$true)][string]$VcInclude,[Parameter(Mandatory=$true)][string]$UcrtInclude,[Parameter(Mandatory=$true)][string]$KitHeaderRoot,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit source-lane324 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if($h -notmatch '^[a-fA-F0-9]{64}$' -or (Sha $p) -cne $h.ToLowerInvariant()){throw "Exact input pin mismatch: $p"}}
Pin "$BaselinePackageDirectory\signed-manifest.json" 'fd1c90d35fde04663a17285cbb69e91b3545ddf54af1e030a77aacd3813626bf'
Pin "$BaselineBuildDirectory\build-manifest.json" 'b474fa33b7a32178592a24577bc8151a059329980663a93cb350d9e214ac7c70'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json;$umd=Get-Content "$BaselineBuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
if($base.schema -cne 'rt81-shader-signature323-independent-sources-v1' -or $umd.shader_delta_source_commit -cne 'eb4dca0ee3134c9a9baea4021325f4a924872444'){throw 'Actual323 UMD source baseline required'}
foreach($e in $base.outputs){Pin (Join-Path $BaselinePackageDirectory $e.name) $e.sha256}
foreach($e in $umd.outputs){Pin (Join-Path $BaselineBuildDirectory $e.name) $e.sha256}
$compiler="$DependencyDirectory\compiler";$pairDir="$DependencyDirectory\pair"
Pin "$compiler\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$pairDir\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
$cm=Get-Content "$compiler\link-manifest.json" -Raw|ConvertFrom-Json;$pm=Get-Content "$pairDir\manifest.json" -Raw|ConvertFrom-Json
if($cm.exit_code -ne 0 -or $cm.source_dirty -or $cm.crt_entry -cne '_DllMainCRTStartup' -or $pm.source_dirty -or !$pm.user_mode_fp -or $pm.kernel_no_fp_applied){throw 'Successful actual compiler/pair closure required'}
$libs=@();foreach($e in $cm.inputs){if($e.role -eq 'verified standard UMD module'){continue};$p=Join-Path $compiler ([IO.Path]::GetFileName($e.path));Pin $p $e.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Exact10 PCO/NIR/CRT/import inputs required'}
Pin "$pairDir\pco_pair.obj" $pm.object_sha256
foreach($name in @('umd/entry','umd/typed_entry')){$e=@($umd.baseline_umd_compiled_units|Where-Object{$_.unit -ceq $name});if($e.Count -ne 1){throw 'Unique retained entry provenance required'};Pin "$BaselineBuildDirectory\baseline\$($name.Replace('/','_')).obj" $e[0].object_sha256}
$expected=(& git -C $root rev-parse '1b4e06958d00f703fa3649648a2c2e7b18f92a0a:drivers/gpu/mt8167/wddm-render/compiler/sm4.c')
if($expected -cne (& git -C $root hash-object "$root\drivers\gpu\mt8167\wddm-render\compiler\sm4.c")){throw 'Reviewed source-lane delta differs'}
& python -B "$PSScriptRoot\verify-ge8300-fullsource.py";if($LASTEXITCODE){throw 'Source origin gate'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $out,"$out\before","$out\after","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 Copy-Item "$compiler\ge8300-pco-arm32.lib" "$out\ge8300-pco-arm32.lib"
 Copy-Item "$BaselineBuildDirectory\ge8300-native-umd.lib" $out
 foreach($name in @('umd_entry.obj','umd_typed_entry.obj')){Copy-Item "$BaselineBuildDirectory\baseline\$name" $out}
 $plan=Get-Content "$root\third_party\mesa-pco\source-plan.json" -Raw|ConvertFrom-Json
 $unit=@($plan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/sm4.c'})
 if($unit.Count -ne 1 -or $unit[0].component -cne 'pco'){throw 'One reviewed PCO compiler unit required'}
 $args=@();foreach($flag in $unit[0].arguments){$s=[string]$flag;$s=$s.Replace('${REPO}',$root.Replace('\','/')).Replace('${VC_INCLUDE}',$VcInclude.Replace('\','/')).Replace('${UCRT_INCLUDE}',$UcrtInclude.Replace('\','/')).Replace('${SDK_SHARED}',($KitHeaderRoot+'\shared').Replace('\','/')).Replace('${SDK_UM}',($KitHeaderRoot+'\um').Replace('\','/'));if($s.Contains('${')){throw 'Unresolved source-plan variable'};$args+=$s}
 $code="$root\drivers\gpu\mt8167\wddm-render\compiler\sm4.c"
 & clang @args -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c $code -o "$out\sm4_adapter.obj"
 if($LASTEXITCODE){throw 'One-TU ARM compiler lane build'}
 $pe=@(& llvm-readobj --file-headers "$out\sm4_adapter.obj");if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'ARMNT compiler object required'}
 $before=@(& llvm-ar t "$out\ge8300-pco-arm32.lib")
 if($before.Count -ne 33 -or @($before|Sort-Object -Unique).Count -ne 33 -or @($before|Where-Object{$_ -ceq 'sm4_adapter.obj'}).Count -ne 1){throw 'Exact unique33-member compiler baseline required'}
 & llvm-ar x "--output=$out\before" "$out\ge8300-pco-arm32.lib";if($LASTEXITCODE){throw 'Extract original compiler members'}
 & llvm-ar r "$out\ge8300-pco-arm32.lib" "$out\sm4_adapter.obj";if($LASTEXITCODE){throw 'Replace one compiler member'}
 $after=@(& llvm-ar t "$out\ge8300-pco-arm32.lib")
 if(($before -join "`n") -cne ($after -join "`n")){throw 'Compiler archive order/count changed'}
 & llvm-ar x "--output=$out\after" "$out\ge8300-pco-arm32.lib";if($LASTEXITCODE){throw 'Extract updated compiler members'}
 $members=@();foreach($n in $before){$a=Sha "$out\before\$n";$b=Sha "$out\after\$n";if(($a -cne $b) -ne ($n -ceq 'sm4_adapter.obj')){throw "Unreviewed compiler member delta: $n"};$members+=@{name=$n;before_sha256=$a;after_sha256=$b;changed=($a -cne $b)}}
 $linked=@($libs|ForEach-Object{if([IO.Path]::GetFileName($_) -ceq 'ge8300-pco-arm32.lib'){"$out\ge8300-pco-arm32.lib"}else{$_}})
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$pairDir\pco_pair.obj" @linked "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD324 relink'}
 $unsigned=Sha "$out\ge8300-umd.dll";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD324 sign'}
 & $SignToolPath verify /pa "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD324 verify'}
 Pin "$out\ge8300-native-umd.lib" (Sha "$BaselineBuildDirectory\ge8300-native-umd.lib")
 [ordered]@{schema='sm4-lane324-compiler-delta-v1';source_commit=$source;functional_source_commit='1b4e06958d00f703fa3649648a2c2e7b18f92a0a';compiler_baseline_manifest_source_commit=$cm.source_commit;compiler_baseline_manifest_sha256=(Sha "$compiler\link-manifest.json");compiler_baseline_archive_sha256=(Sha "$compiler\ge8300-pco-arm32.lib");umd_baseline_package_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");umd_baseline_build_manifest_sha256=(Sha "$BaselineBuildDirectory\build-manifest.json");pair_manifest_sha256=(Sha "$pairDir\manifest.json");compiled_units=@('compiler/sm4.c');compile_arguments=$args;source_sha256=(Sha $code);archive_members=$members;changed_member_count=1;retained_other_members=32;umd_archive_rebuilt=$false;umd_archive_byte_retained=$true;nir_util_support_crt_pair_rebuilt=$false;pco_full_rebuilt=$false;kernel_rebuilt=$false;native_rebuilt=$false;client_rebuilt=$false;sdk_installed=$false;unsigned_dll_sha256=$unsigned;signature_status='Valid';signer_thumbprint=$thumb;hardware_verified=$false;outputs=@('ge8300-umd.dll','ge8300-pco-arm32.lib','ge8300-native-umd.lib','sm4_adapter.obj','umd_entry.obj','umd_typed_entry.obj')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
