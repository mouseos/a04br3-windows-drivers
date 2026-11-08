param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$DependencyDirectory,[Parameter(Mandatory=$true)][string]$KernelKitRoot,[Parameter(Mandatory=$true)][string]$UmHeaderRoot,[Parameter(Mandatory=$true)][string]$MicrosoftLinkPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';if(!$EnableCandidate){throw 'Explicit filter327 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if(@(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
& python -B "$root\tools\ci\verify-ge8300-fullsource.py";if($LASTEXITCODE){throw 'Normal full-source gate'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Pin mismatch $p"}}

Pin "$BaselineBuildDirectory\build-manifest.json" 'e590285abbf98d729aaaa6c38dea02988e80a346c50ceb1b7c6b5af75d1a5a3d'
Pin "$BaselinePackageDirectory\signed-manifest.json" '38fc11b911da7e43eac495a945774b0854479447f1d521cdeaa96bff6c7beb80'
$bm=Get-Content "$BaselineBuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json
foreach($e in $bm.outputs){Pin "$BaselineBuildDirectory\$($e.name)" $e.sha256}
foreach($e in $base.outputs){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
$compiler="$DependencyDirectory\compiler";$pair="$DependencyDirectory\pair"
Pin "$compiler\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$pair\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
$cm=Get-Content "$compiler\link-manifest.json" -Raw|ConvertFrom-Json;$pm=Get-Content "$pair\manifest.json" -Raw|ConvertFrom-Json
$libs=@();foreach($e in $cm.inputs){if($e.role -eq 'verified standard UMD module'){continue};$p=Join-Path $compiler ([IO.Path]::GetFileName($e.path));Pin $p $e.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Exact compiler closure10'};Pin "$pair\pco_pair.obj" $pm.object_sha256
$out=[IO.Path]::GetFullPath($OutputDirectory);if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'New checkout-local output'}
New-Item -ItemType Directory $out,"$out\tmp","$out\native","$out\kmd","$out\umd","$out\native-before","$out\kmd-before","$out\umd-before"|Out-Null
$nativeArgs=@(($bm.compiled_units|Where-Object{$_.role -ceq 'native'}|Select-Object -First 1).arguments|ForEach-Object{([string]$_).Replace('C:\mt8167-texture326\source',$root).Replace('C:\mt8167-blacktriangle325\source',$root)})
$umdArgs=@(($bm.compiled_units|Where-Object{$_.role -ceq 'UMD-dependent-closure'}|Select-Object -First 1).arguments)
$kmdArgs=@(($bm.compiled_units|Where-Object{$_.role -ceq 'KMD-dependent-closure'}|Select-Object -First 1).arguments)
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
 $nm=@(Extract "$BaselineBuildDirectory\ge8300-native-owner-core.lib" 'native');$nc=@()
 foreach($unit in @('native_draw_job','native_texture2d_state')){$name="vendor_m1_11-$unit.obj";Compile "drivers/gpu/mt8167/vendor_m1_11/render_owner/$unit.c" "$out\native\$name" $nativeArgs 'native';$nc+=$name}
 $name='wddm-render-frontend-texture_draw_protocol.obj';Compile 'drivers/gpu/mt8167/wddm-render/frontend/texture_draw_protocol.c' "$out\native\$name" $nativeArgs 'native';$nc+=$name
 Archive $nm 'native' 'ge8300-native-owner-core.lib' $nc
 $km=@(Extract "$BaselineBuildDirectory\ge8300-pair-kmd.lib" 'kmd');Compile 'drivers/gpu/mt8167/wddm-render/query_adapter.c' "$out\kmd\query_adapter.obj" $kmdArgs 'KMD';Archive $km 'kmd' 'ge8300-pair-kmd.lib' @('query_adapter.obj')
 $um=@(Extract "$BaselineBuildDirectory\ge8300-native-umd.lib" 'umd');$uc=@()
 foreach($unit in @('frontend/texture_draw_protocol','umd/typed_texture','umd/typed_draw','umd/frontend','umd/typed_entry')){
  $name=$unit.Replace('/','_')+'.obj';$f=@($umdArgs)
  if($unit -eq 'umd/typed_entry'){$f+=@('-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1')}
  Compile "drivers/gpu/mt8167/wddm-render/$unit.c" "$out\umd\$name" $f 'UMD'
  if($unit -ne 'umd/typed_entry'){$uc+=$name}
 }
 Copy-Item "$BaselineBuildDirectory\umd\umd_entry.obj" "$out\umd\umd_entry.obj"
 Archive $um 'umd' 'ge8300-native-umd.lib' $uc
 $kernelLibs=@('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')|ForEach-Object{"$KernelKitRoot\Lib\winv6.3\km\arm\$_"}
 & $MicrosoftLinkPath /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$out\ge8300-pair-kmd.lib" "$out\ge8300-native-owner-core.lib" @kernelLibs "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'SYS link'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd\umd_entry.obj" "$out\umd\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$pair\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD link'}
 $clientArgs=@($umdArgs|Where-Object{$_ -notin @('-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS')})
 Compile 'drivers/gpu/mt8167/wddm-render/testclient/d3d-texture.c' "$out\testclient_d3d-texture.obj" $clientArgs 'new-standard-client'
 Compile 'drivers/gpu/mt8167/wddm-render/testclient/memory.c' "$out\testclient_memory.obj" $clientArgs 'new-standard-client-support'
 $umlibs='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm'
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d-texture.obj" "$out\testclient_memory.obj" "$umlibs\kernel32.lib" "$umlibs\setupapi.lib" "$umlibs\dxgi.lib" "$umlibs\d3d11.lib" "/out:$out\ge8300-d3d-texture.exe"
 if($LASTEXITCODE){throw 'Client link'}
 foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-texture.exe')){& $SignToolPath sign /fd SHA256 /sha1 BCBF6C456C6F0DD0818434763B189488FBF13570 "$out\$n";if($LASTEXITCODE){throw 'Sign'};& $SignToolPath verify /pa "$out\$n";if($LASTEXITCODE){throw 'Verify'}}
 [ordered]@{schema='texture-filter327-incremental-v1';source_commit=$source;baseline326_signed_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");compiled_units=$records;archives=$archives;existing_compiler_libraries=@($libs|ForEach-Object{@{name=[IO.Path]::GetFileName($_);sha256=(Sha $_)}});PCO_rebuilt=$false;SDK_rebuilt=$false;old_client_rebuilt=$false;native_IRQ_reset_profile_changed=$false;draw_layout_version=3;sampler_profile_version=4;adapter_contract_version=4;texture_storage_version=2;normal_fullsource_gate_passed=$true;normal_fullsource_compiler_units=372;hardware_verified=$false;standard_profile_qualified=$false;outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-texture.exe','ge8300-native-owner-core.lib','ge8300-pair-kmd.lib','ge8300-native-umd.lib')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
