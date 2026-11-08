# Reconstruct the lost 3f8291d0 UMD archive from source using existing compiler/
# CRT/pair only; then replace exactly the tokenized shader wrapper member.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$CompilerDirectory,[Parameter(Mandatory=$true)][string]$PairDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$ReconstructedBaselineDirectory,[string]$ReconstructedBaselineManifestSha256)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit 323 development opt-in required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Exact input pin mismatch: $p"}}
$baseCommit='3f8291d094c01206dfaa0a22d1fdccd6eecc5823';$deltaCommit='eb4dca0ee3134c9a9baea4021325f4a924872444'
Pin "$CompilerDirectory\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$PairDirectory\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
Pin "$BaselinePackageDirectory\signed-manifest.json" '59bb556d372d923c9b7b8bfb3ea08b61f9d3f5c18c851f8f9e8750f2c7185a29'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
if($base.schema -cne 'implemented-reset322-independent-sources-v1' -or $base.umd_source_commit -cne $baseCommit -or $base.native_owner_source_commit -cne 'f96948533601f9fdcdcc22b9700aacd96dbf7204'){throw '322 independent source baseline required'}
foreach($e in $base.outputs){Pin (Join-Path $BaselinePackageDirectory $e.name) $e.sha256}
Pin "$BaselinePackageDirectory\trace-signed-manifest.json" '6bc00603aea90762a04b125667645979c1887d40e4632e0105d44a51e449a9a3'
$cm=Get-Content "$CompilerDirectory\link-manifest.json" -Raw|ConvertFrom-Json
if($cm.exit_code -ne 0 -or $cm.source_dirty -or $cm.crt_entry -cne '_DllMainCRTStartup'){throw 'Fixed successful compiler provenance required'}
$libs=@();foreach($e in $cm.inputs){if($e.role -eq 'verified standard UMD module'){continue};$p=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($e.path));Pin $p $e.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Exact 10 compiler/CRT closure inputs required'}
$pair=Get-Content "$PairDirectory\manifest.json" -Raw|ConvertFrom-Json
if($pair.source_dirty -or !$pair.user_mode_fp -or $pair.kernel_no_fp_applied){throw 'Exact pair provenance required'}
Pin "$PairDirectory\pco_pair.obj" $pair.object_sha256
$component='drivers/gpu/mt8167/wddm-render/'
foreach($p in @('umd/tokenized_shader.h','compiler/shader_compiler.h','compiler/pair_compiler.h','compiler/sm2.h','compiler/sm4.h','clear_protocol.h','memory.h')){
 $old=(& git -C $root rev-parse ($baseCommit+':'+$component+$p));$now=(& git -C $root hash-object ($root+'/'+$component+$p));if($old -cne $now){throw "Unreviewed affected header: $p"}
}
$expected=(& git -C $root rev-parse ($deltaCommit+':'+$component+'umd/tokenized_shader.c'))
if($expected -cne (& git -C $root hash-object "$root/$component/umd/tokenized_shader.c")){throw 'Delta source differs from reviewed eb4dca0e'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
New-Item -ItemType Directory $out,"$out\tmp","$out\baseline","$out\before","$out\after"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 # A separate source-local worktree has the exact entire 3f8291d0 UMD tree.
 $baseSource="$out\baseline-source"
 & git -c core.autocrlf=false -c core.eol=lf -C $root worktree add --detach $baseSource $baseCommit
 if($LASTEXITCODE){throw 'Exact baseline UMD source worktree failed'}
 $inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002',"-I$inc\shared","-I$inc\um","-I$inc\km",'-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration')
 $units=@('clear_protocol','frontend/draw_protocol','frontend/draw_link_protocol','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','umd/typed_state','umd/typed_resource','umd/typed_view','umd/typed_shader','umd/typed_draw','umd/typed_pair','umd/typed_runtime','umd/typed_dxgi','umd/typed_entry','umd/entry')
 $objects=@();$compiled=@()
 if($ReconstructedBaselineDirectory){
  Pin "$ReconstructedBaselineDirectory\reconstruction-manifest.json" $ReconstructedBaselineManifestSha256
  $rr=Get-Content "$ReconstructedBaselineDirectory\reconstruction-manifest.json" -Raw|ConvertFrom-Json
  if($rr.schema -cne 'rt323-baseline-reconstruction-recovery-v1' -or $rr.source_commit -cne $baseCommit -or $rr.original_helper_source_commit -cne '8f42e99bfb77c59817defca229c211569de600aa' -or $rr.compiled.Count -ne $units.Count){throw 'Exact prior source-only reconstruction receipt required'}
  Pin "$ReconstructedBaselineDirectory\ge8300-native-umd.lib" $rr.archive_sha256
  foreach($u in $units){
   $e=@($rr.compiled|Where-Object{$_.unit -ceq $u});if($e.Count -ne 1){throw 'Unique reconstructed object required'}
   $obj=$u.Replace('/','_')+'.obj'
   Pin "$baseSource/$component$u.c" $e[0].source_sha256
   Pin "$ReconstructedBaselineDirectory\$obj" $e[0].object_sha256
   Copy-Item "$ReconstructedBaselineDirectory\$obj" "$out\baseline\$obj"
  }
  Copy-Item "$ReconstructedBaselineDirectory\ge8300-native-umd.lib" "$out\baseline\ge8300-native-umd.lib"
  Copy-Item "$ReconstructedBaselineDirectory\reconstruction-manifest.json" "$out\baseline\reconstruction-manifest.json"
  $compiled=@($rr.compiled)
 }else{
 foreach($u in $units){
  $flags=@($common);if($u -ne 'umd/typed_state'){$flags+=@('-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS')}
  if($u -eq 'umd/typed_entry'){$flags+=@('-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1')}
  if($u -eq 'umd/entry'){$flags+='-DGEO_LINKED_REAL_FRONTEND=1'}
  $p="$baseSource/$component$u.c";$obj="$out\baseline\$($u.Replace('/','_')).obj"
  & clang @flags -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c $p -o $obj
  if($LASTEXITCODE){throw "Baseline UMD-only compile: $u"}
  $compiled+=@{unit=$u;source_sha256=(Sha $p);object_sha256=(Sha $obj)}
  if($u -notin @('umd/typed_entry','umd/entry')){$objects+=$obj}
 }
 & llvm-lib /machine:arm "/out:$out\baseline\ge8300-native-umd.lib" @objects
 if($LASTEXITCODE){throw 'Baseline UMD archive reconstruction'}
 }
 $reconstructed=Sha "$out\baseline\ge8300-native-umd.lib"
 Copy-Item "$out\baseline\ge8300-native-umd.lib" $out
 $flags=@($common)+@('-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS')
 & clang @flags -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c "$root/$component/umd/tokenized_shader.c" -o "$out\umd_tokenized_shader.obj"
 if($LASTEXITCODE){throw 'Delta tokenized_shader ARM compile'}
 $beforeRaw=@(& llvm-ar t "$out\ge8300-native-umd.lib");$before=@($beforeRaw|ForEach-Object{[IO.Path]::GetFileName($_)})
 if($before.Count -ne @($before|Sort-Object -Unique).Count -or @($before|Where-Object{$_ -ceq 'umd_tokenized_shader.obj'}).Count -ne 1){throw 'Archive members must be unique with exactly one target'}
 & llvm-ar x "--output=$out\before" "$out\ge8300-native-umd.lib";if($LASTEXITCODE){throw 'Extract baseline members'}
 & llvm-ar r "$out\ge8300-native-umd.lib" "$out\umd_tokenized_shader.obj";if($LASTEXITCODE){throw 'Replace exactly one member'}
 $afterRaw=@(& llvm-ar t "$out\ge8300-native-umd.lib");$after=@($afterRaw|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($after -join "`n") -cne ($before -join "`n")){throw 'Archive member order/count changed'}
 & llvm-ar x "--output=$out\after" "$out\ge8300-native-umd.lib";if($LASTEXITCODE){throw 'Extract new members'}
 $members=@();foreach($n in $before){$a=Sha "$out\before\$n";$b=Sha "$out\after\$n";if(($a -cne $b) -ne ($n -ceq 'umd_tokenized_shader.obj')){throw "Unexpected member delta: $n"};$members+=@{name=$n;before_sha256=$a;after_sha256=$b;changed=($a -cne $b)}}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\baseline\umd_entry.obj" "$out\baseline\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$PairDirectory\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw '323 UMD link'}
 $unsigned=Sha "$out\ge8300-umd.dll";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign="${env:ProgramFiles(x86)}\Windows Kits\8.1\bin\x86\signtool.exe"
 & $sign sign /fd SHA256 /sha1 $thumb "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD sign'}
 & $sign verify /pa "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'UMD verify'}
 [ordered]@{schema='rt81-shader-signature323-umd-build-v1';source_commit=$source;shader_delta_source_commit=$deltaCommit;umd_baseline_source_commit=$baseCommit;baseline_archive_reconstructed=$true;baseline_reconstruction_reused=[bool]$ReconstructedBaselineDirectory;baseline_umd_compiled_units=$compiled;historical_baseline_archive_sha256='af27b858a6e1d5706693b45a4519d07cc04bc84858663345b390edba059211ea';reconstructed_baseline_archive_sha256=$reconstructed;historical_baseline_archive_byte_equal=($reconstructed -ceq 'af27b858a6e1d5706693b45a4519d07cc04bc84858663345b390edba059211ea');delta_compiled_units=@('umd/tokenized_shader');archive_members=$members;changed_member_count=@($members|Where-Object{$_.changed}).Count;compiler_manifest_sha256=(Sha "$CompilerDirectory\link-manifest.json");pair_manifest_sha256=(Sha "$PairDirectory\manifest.json");compiler_rebuilt=$false;pair_rebuilt=$false;client_rebuilt=$false;kernel_rebuilt=$false;native_rebuilt=$false;unsigned_dll_sha256=$unsigned;signature_status='Valid';signer_thumbprint=$thumb;hardware_verified=$false;outputs=@('ge8300-native-umd.lib','ge8300-umd.dll','umd_tokenized_shader.obj')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
