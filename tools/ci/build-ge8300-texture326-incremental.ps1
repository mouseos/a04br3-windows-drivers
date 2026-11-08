# BGRA8 POINT+CLAMP candidate. No hardware or SDK/PCO rebuild.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$DependencyDirectory,[Parameter(Mandatory=$true)][string]$KernelKitRoot,[Parameter(Mandatory=$true)][string]$UmHeaderRoot,[Parameter(Mandatory=$true)][string]$VcInclude,[Parameter(Mandatory=$true)][string]$UcrtInclude,[Parameter(Mandatory=$true)][string]$MicrosoftLinkPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit texture candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Pin mismatch $p"}}
Pin "$BaselineBuildDirectory\build-manifest.json" '1da373f2106d04bb95d1bbe19603b9372572f97251f8b4b8cd691254fa1651b5'
Pin "$BaselinePackageDirectory\signed-manifest.json" '8e595170ddec23335e9fb2f7031f45e9eba5f393adae32df01c10b93cc896ca0'
$bm=Get-Content "$BaselineBuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
foreach($e in $bm.outputs){Pin "$BaselineBuildDirectory\$($e.name)" $e.sha256}
foreach($e in $base.outputs){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
$compiler="$DependencyDirectory\compiler";$pair="$DependencyDirectory\pair"
Pin "$compiler\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$pair\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
$cm=Get-Content "$compiler\link-manifest.json" -Raw|ConvertFrom-Json;$pm=Get-Content "$pair\manifest.json" -Raw|ConvertFrom-Json
$libs=@();foreach($e in $cm.inputs){if($e.role -eq 'verified standard UMD module'){continue};$p=Join-Path $compiler ([IO.Path]::GetFileName($e.path));Pin $p $e.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Exact existing compiler closure10 required'};Pin "$pair\pco_pair.obj" $pm.object_sha256
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp","$out\native","$out\kmd","$out\umd","$out\native-before","$out\kmd-before","$out\umd-before"|Out-Null
$gpu="$root\drivers\gpu\mt8167";$component="$gpu\wddm-render";$kinc="$KernelKitRoot\Include";$resource=(& clang -print-resource-dir)
$nativeArgs=@($bm.native_compile_arguments|ForEach-Object{([string]$_).Replace('C:\mt8167-blacktriangle325\source',$root)})
$umdArgs=@($bm.umd_compile_arguments)
$kmdArgs=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DDXGKDDI_INTERFACE_VERSION=0x4002','-DD3D_UMD_INTERFACE_VERSION=0x4002',"-I$kinc\shared",'-DGEO_LINKED_NATIVE_CORE=1','-D_KERNEL_MODE','-D__ldrexd=__geo_wdk81_ldrexd_declaration',"-I$resource\include","-I$kinc\km","-I$kinc\km\crt",'-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type')
$records=@();$archives=@()
function Extract($lib,$tag){
 $members=@(& llvm-ar t $lib|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(@($members|Sort-Object -Unique).Count -ne $members.Count){throw 'Duplicate archive leaf'}
 & llvm-ar x "--output=$out\$tag-before" $lib;if($LASTEXITCODE){throw 'Archive extract'}
 foreach($m in $members){if(!(Test-Path "$out\$tag-before\$m")){throw "Missing original member $m"};Copy-Item "$out\$tag-before\$m" "$out\$tag\$m"}
 return $members
}
function Compile($path,$obj,$flags,$role){
 & clang @flags -c "$root\$path" -o $obj;if($LASTEXITCODE){throw "Compile $path"}
 $script:records+=@{source=$path;source_sha256=(Sha "$root\$path");object=[IO.Path]::GetFileName($obj);object_sha256=(Sha $obj);role=$role;arguments=$flags}
}
function Archive($members,$tag,$leaf,$changed){
 $rows=@();foreach($m in $members){$after=Sha "$out\$tag\$m";$before=$null;if(Test-Path "$out\$tag-before\$m"){$before=Sha "$out\$tag-before\$m"}
  if($before -cne $after -and $m -notin $changed){throw "Unexpected member change $tag/$m"}
  $rows+=@{name=$m;before_sha256=$before;sha256=$after;compiled=($m -in $changed)}
 }
 $paths=@($members|ForEach-Object{"$out\$tag\$_"});[array]::Reverse($paths);& llvm-lib /machine:arm "/out:$out\$leaf" @paths;if($LASTEXITCODE){throw 'Archive link'}
 $made=@(& llvm-ar t "$out\$leaf"|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($made -join "`n") -cne ($members -join "`n")){throw 'Archive order/member count'}
 $script:archives+=@{role=$tag;file=$leaf;sha256=(Sha "$out\$leaf");members=$rows}
}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $nm=@(Extract "$BaselineBuildDirectory\ge8300-native-owner-core.lib" 'native')
 $nc=@()
 foreach($unit in @('native_draw_job','windows_native_job','native_owner_factory','native_texture2d_state','native_texture2d_program')){
  $name="vendor_m1_11-$unit.obj";Compile "drivers/gpu/mt8167/vendor_m1_11/render_owner/$unit.c" "$out\native\$name" $nativeArgs 'native';$nc+=$name;if($name -notin $nm){$nm+=$name}
 }
 foreach($unit in @('clear_protocol','frontend/texture_draw_protocol','compiler/texture2d_sm4')){
  $name='wddm-render-'+$unit.Replace('/','-')+'.obj';Compile "drivers/gpu/mt8167/wddm-render/$unit.c" "$out\native\$name" $nativeArgs 'native';$nc+=$name;if($name -notin $nm){$nm+=$name}
 }
 Archive $nm 'native' 'ge8300-native-owner-core.lib' $nc
 $km=@(Extract "$BaselineBuildDirectory\ge8300-pair-kmd.lib" 'kmd');$kc=@()
 foreach($unit in @('clear_protocol','frontend/draw_protocol','clear_engine','capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory')){
  $name=$unit.Replace('/','_')+'.obj';Compile "drivers/gpu/mt8167/wddm-render/$unit.c" "$out\kmd\$name" $kmdArgs 'KMD-dependent-closure';$kc+=$name
 }
 Archive $km 'kmd' 'ge8300-pair-kmd.lib' $kc
 $um=@(Extract "$BaselineBuildDirectory\ge8300-native-umd.lib" 'umd');$uc=@()
 foreach($unit in @('clear_protocol','frontend/draw_protocol','frontend/draw_link_protocol','frontend/texture_draw_protocol','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','compiler/texture2d_sm4','umd/typed_state','umd/typed_resource','umd/typed_view','umd/typed_shader','umd/typed_draw','umd/typed_pair','umd/typed_runtime','umd/typed_dxgi','umd/typed_texture','umd/typed_entry','umd/entry')){
  $name=$unit.Replace('/','_')+'.obj';$f=@($umdArgs)
  if($unit -eq 'umd/typed_state'){$f=@($f|Where-Object{$_ -notin @('-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS')})}
  if($unit -eq 'umd/typed_entry'){$f+=@('-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1')}
  if($unit -eq 'umd/entry'){$f+='-DGEO_LINKED_REAL_FRONTEND=1'}
  Compile "drivers/gpu/mt8167/wddm-render/$unit.c" "$out\umd\$name" $f 'UMD-dependent-closure';$uc+=$name
  if($unit -notin @('umd/entry','umd/typed_entry') -and $name -notin $um){$um+=$name}
 }
 $plan=Get-Content "$root\third_party\mesa-pco\source-plan.json" -Raw|ConvertFrom-Json
 $recipe=@($plan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c'});if($recipe.Count -ne 1){throw 'Compiler adapter recipe'}
 $cf=@();foreach($a in $recipe[0].arguments){$s=([string]$a).Replace('${REPO}',$root.Replace('\','/')).Replace('${VC_INCLUDE}',$VcInclude.Replace('\','/')).Replace('${UCRT_INCLUDE}',$UcrtInclude.Replace('\','/')).Replace('${SDK_SHARED}',($UmHeaderRoot+'\shared').Replace('\','/')).Replace('${SDK_UM}',($UmHeaderRoot+'\um').Replace('\','/'));if($s.Contains('${')){throw 'Unexpanded compiler argument'};$cf+=$s}
 Compile 'drivers/gpu/mt8167/wddm-render/compiler/pco_texture2d.c' "$out\umd\compiler_pco_texture2d.obj" $cf 'source-compiled-SAMPLE-adapter';$um+='compiler_pco_texture2d.obj';$uc+='compiler_pco_texture2d.obj'
 Archive $um 'umd' 'ge8300-native-umd.lib' $uc
 $kernelLibs=@('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')|ForEach-Object{"$KernelKitRoot\Lib\winv6.3\km\arm\$_"}
 & $MicrosoftLinkPath /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$out\ge8300-pair-kmd.lib" "$out\ge8300-native-owner-core.lib" @kernelLibs "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'SYS link'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd\umd_entry.obj" "$out\umd\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$pair\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD link'}
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 foreach($n in @('ge8300-render.sys','ge8300-umd.dll')){& $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\$n";if($LASTEXITCODE){throw 'Sign'};& $SignToolPath verify /pa "$out\$n";if($LASTEXITCODE){throw 'Verify'}}
 [ordered]@{schema='texture326-dependency-closure-build-v1';source_commit=$source;baseline325_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");baseline325_build_manifest_sha256=(Sha "$BaselineBuildDirectory\build-manifest.json");compiled_units=$records;archives=$archives;existing_compiler_libraries=@($libs|ForEach-Object{@{name=[IO.Path]::GetFileName($_);sha256=(Sha $_)}});pair_object_sha256=(Sha "$pair\pco_pair.obj");driver_wire_version=3;texture_storage_version=2;adapter_contract_version=3;native_resolve_ABI_version=1;SDK_rebuilt=$false;PCO_rebuilt=$false;CRT_rebuilt=$false;client_rebuilt=$false;hardware_verified=$false;standard_profile_qualified=$false;outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-native-owner-core.lib','ge8300-pair-kmd.lib','ge8300-native-umd.lib')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 15|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
