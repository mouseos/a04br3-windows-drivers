# Independent325: PPP viewport order repair + bounded first-draw input trace.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$KernelInputDirectory,[Parameter(Mandatory=$true)][string]$BaselineBuildDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackageDirectory,[Parameter(Mandatory=$true)][string]$DependencyDirectory,[Parameter(Mandatory=$true)][string]$KernelKitRoot,[Parameter(Mandatory=$true)][string]$UmHeaderRoot,[Parameter(Mandatory=$true)][string]$MicrosoftLinkPath,[Parameter(Mandatory=$true)][string]$SignToolPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit325 candidate required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$source=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h.ToLowerInvariant()){throw "Exact pin mismatch: $p"}}
$allowed=@('drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.c','drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h','drivers/gpu/mt8167/wddm-render/umd/typed_draw.c','tests/ge8300_ppp_viewport_order_test.c','docs/ge8300-blacktriangle119-viewport.md','tools/ci/build-blacktriangle325-incremental.ps1','tools/ci/package-blacktriangle325.ps1')
foreach($p in @(& git -C $root diff --name-only 10430631c40be8a801e4b948a7d0bc7ca707516d HEAD)){if($p -notin $allowed){throw "Unreviewed325 delta: $p"}}
Pin "$KernelInputDirectory\ge8300-native-owner-core.lib" '0ebf455c975bd80b838dca3199b06f010a0d282d043ec6db68ec077546d82ce4'
Pin "$KernelInputDirectory\ge8300-pair-kmd.lib" '3c818dba1d5b567a21ce1e1f594ebfd9fe03b0f26acf22f83978fe92cf29e4ce'
Pin "$BaselinePackageDirectory\signed-manifest.json" 'fd1c90d35fde04663a17285cbb69e91b3545ddf54af1e030a77aacd3813626bf'
Pin "$BaselineBuildDirectory\build-manifest.json" 'b474fa33b7a32178592a24577bc8151a059329980663a93cb350d9e214ac7c70'
$base=Get-Content "$BaselinePackageDirectory\signed-manifest.json" -Raw|ConvertFrom-Json;$umd=Get-Content "$BaselineBuildDirectory\build-manifest.json" -Raw|ConvertFrom-Json
foreach($e in $base.outputs){Pin "$BaselinePackageDirectory\$($e.name)" $e.sha256}
foreach($e in $umd.outputs){Pin "$BaselineBuildDirectory\$($e.name)" $e.sha256}
$compiler="$DependencyDirectory\compiler";$pair="$DependencyDirectory\pair"
Pin "$compiler\link-manifest.json" '3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b'
Pin "$pair\manifest.json" 'c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a'
$cm=Get-Content "$compiler\link-manifest.json" -Raw|ConvertFrom-Json;$pm=Get-Content "$pair\manifest.json" -Raw|ConvertFrom-Json
$libs=@();foreach($e in $cm.inputs){if($e.role -eq 'verified standard UMD module'){continue};$p=Join-Path $compiler ([IO.Path]::GetFileName($e.path));Pin $p $e.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Exact compiler closure10 required'};Pin "$pair\pco_pair.obj" $pm.object_sha256
foreach($name in @('umd/entry','umd/typed_entry')){$e=@($umd.baseline_umd_compiled_units|Where-Object{$_.unit -ceq $name});if($e.Count -ne 1){throw 'Entry origin'};Pin "$BaselineBuildDirectory\baseline\$($name.Replace('/','_')).obj" $e[0].object_sha256}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp","$out\native-before","$out\native-after","$out\umd-before","$out\umd-after"|Out-Null
function Delta($lib,$object,$member,$label){
 $before=@(& llvm-ar t $lib|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(@($before|Where-Object{$_ -ceq $member}).Count -ne 1 -or @($before|Sort-Object -Unique).Count -ne $before.Count){throw 'Unique archive members required'}
 & llvm-ar x "--output=$out\$label-before" $lib;if($LASTEXITCODE){throw 'Extract before'}
 & llvm-ar r $lib $object;if($LASTEXITCODE){throw 'Replace one member'}
 $after=@(& llvm-ar t $lib|ForEach-Object{[IO.Path]::GetFileName($_)})
 if(($before -join "`n") -cne ($after -join "`n")){throw 'Archive order/count changed'}
 & llvm-ar x "--output=$out\$label-after" $lib;if($LASTEXITCODE){throw 'Extract after'}
 $rows=@();foreach($n in $before){$a=Sha "$out\$label-before\$n";$b=Sha "$out\$label-after\$n";if(($a -cne $b) -ne ($n -ceq $member)){throw "Unreviewed member delta: $n"};$rows+=@{name=$n;before_sha256=$a;after_sha256=$b;changed=($a -cne $b)}}
 return $rows
}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $gpu="$root\drivers\gpu\mt8167";$kinc="$KernelKitRoot\Include";$km="$KernelKitRoot\Lib\winv6.3\km\arm";$resource=(& clang -print-resource-dir)
 $nativeArgs=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1','-D__ldrexd=__geo_wdk81_ldrexd_declaration','-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1','-DA04_GE8300_NATIVE_OWNER_DRAW=1',"-I$resource\include","-I$kinc\km","-I$kinc\km\crt","-I$kinc\shared","-I$gpu","-I$gpu\fwif","-I$gpu\compat","-I$gpu\vendor_m1_11")
 $ns="$gpu\vendor_m1_11\render_owner\native_d3d_viewport.c";$no="$out\vendor_m1_11-native_d3d_viewport.obj"
 & clang @nativeArgs -c $ns -o $no;if($LASTEXITCODE){throw 'Native viewport1TU compile'}
 Copy-Item "$KernelInputDirectory\ge8300-native-owner-core.lib" $out;Copy-Item "$KernelInputDirectory\ge8300-pair-kmd.lib" $out
 $nativeDelta=@(Delta "$out\ge8300-native-owner-core.lib" $no 'vendor_m1_11-native_d3d_viewport.obj' 'native')
 $kernelLibs=@('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')|ForEach-Object{"$km\$_"}
 & $MicrosoftLinkPath /nologo /driver /subsystem:native,6.03 /machine:arm /entry:DriverEntry /nodefaultlib /incremental:no /base:0x400000 "$out\ge8300-pair-kmd.lib" "$out\ge8300-native-owner-core.lib" @kernelLibs "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'Microsoft ARM kernel relink'}
 $umdArgs=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002',"-I$UmHeaderRoot\shared","-I$UmHeaderRoot\um","-I$UmHeaderRoot\km","-I$kinc\km",'-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration','-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types')
 $us="$gpu\wddm-render\umd\typed_draw.c";$uo="$out\umd_typed_draw.obj"
 & clang @umdArgs -c $us -o $uo;if($LASTEXITCODE){throw 'Bounded input trace1TU compile'}
 Copy-Item "$BaselineBuildDirectory\ge8300-native-umd.lib" $out
 $umdDelta=@(Delta "$out\ge8300-native-umd.lib" $uo 'umd_typed_draw.obj' 'umd')
 foreach($n in @('umd_entry.obj','umd_typed_entry.obj')){Copy-Item "$BaselineBuildDirectory\baseline\$n" $out}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$pair\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD trace relink'}
 $unsignedSys=Sha "$out\ge8300-render.sys";$unsignedDll=Sha "$out\ge8300-umd.dll";$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 foreach($n in @('ge8300-render.sys','ge8300-umd.dll')){& $SignToolPath sign /fd SHA256 /sha1 $thumb "$out\$n";if($LASTEXITCODE){throw "Sign $n"};& $SignToolPath verify /pa "$out\$n";if($LASTEXITCODE){throw "Verify $n"}}
 [ordered]@{schema='blacktriangle325-two-tu-build-v1';source_commit=$source;baseline_native_source_commit='f96948533601f9fdcdcc22b9700aacd96dbf7204';kernel_binary_source_commit='f73da1e31755e08727958ac6f9160a0d5ccdff6b';baseline323_manifest_sha256=(Sha "$BaselinePackageDirectory\signed-manifest.json");baseline_native_library_sha256=(Sha "$KernelInputDirectory\ge8300-native-owner-core.lib");baseline_kernel_library_sha256=(Sha "$KernelInputDirectory\ge8300-pair-kmd.lib");baseline_umd_build_manifest_sha256=(Sha "$BaselineBuildDirectory\build-manifest.json");compiler_manifest_sha256=(Sha "$compiler\link-manifest.json");pair_manifest_sha256=(Sha "$pair\manifest.json");native_compiled_units=@('native_d3d_viewport');umd_compiled_units=@('umd/typed_draw');native_source_sha256=(Sha $ns);umd_source_sha256=(Sha $us);native_archive_members=$nativeDelta;umd_archive_members=$umdDelta;native_compile_arguments=$nativeArgs;umd_compile_arguments=$umdArgs;linker_sha256=(Sha $MicrosoftLinkPath);kernel_imports=@($kernelLibs|ForEach-Object{@{name=([IO.Path]::GetFileName($_));sha256=(Sha $_)}});unsigned_sys_sha256=$unsignedSys;unsigned_dll_sha256=$unsignedDll;changed_wire_abi=$false;changed_caps=$false;compiler_rebuilt=$false;client_rebuilt=$false;sdk_installed=$false;hardware_verified=$false;signer_thumbprint=$thumb;signature_status='Valid';outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-native-owner-core.lib','ge8300-native-umd.lib','ge8300-pair-kmd.lib','vendor_m1_11-native_d3d_viewport.obj','umd_typed_draw.obj','umd_entry.obj','umd_typed_entry.obj')|ForEach-Object{@{name=$_;sha256=(Sha "$out\$_")}}}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
