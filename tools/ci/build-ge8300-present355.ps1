param([Parameter(Mandatory=$true)][string]$TaskRoot)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Pin($p,$h){if((Sha $p) -cne $h){throw "Pin mismatch $p"}}
$root="$TaskRoot\source";$cache="$TaskRoot\cache";$out="$TaskRoot\build"
if(Test-Path $out){throw 'New output directory required'}
New-Item -ItemType Directory $out,"$out\objects","$out\deps"|Out-Null
foreach($r in (Get-Content "$cache\cache-input-pins.json" -Raw|ConvertFrom-Json)){Pin "$cache\$($r.name)" $r.sha256}
foreach($r in (Get-Content "$root\source-pins.json" -Raw|ConvertFrom-Json)){Pin "$root\$($r.name)" $r.sha256}
$base=Get-Content "$cache\baseline\build-manifest.json" -Raw|ConvertFrom-Json
$archive=@($base.archives|Where-Object{$_.role -ceq 'umd'});if($archive.Count -ne 1){throw 'One baseline UMD archive'}
Pin "$cache\baseline\ge8300-native-umd.lib" $archive[0].sha256
$members=@(& llvm-ar t "$cache\baseline\ge8300-native-umd.lib"|ForEach-Object{[IO.Path]::GetFileName($_)})
if(($members -join '|') -cne (($archive[0].members|ForEach-Object{$_.name}) -join '|')){throw 'Real archive order differs'}
& llvm-ar x "--output=$out\objects" "$cache\baseline\ge8300-native-umd.lib";if($LASTEXITCODE){throw 'Extract'}
foreach($r in $archive[0].members){Pin "$out\objects\$($r.name)" $r.sha256}
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002','-IC:\mt8167-minimal-service-host\cache\kit81\Include\shared','-IC:\mt8167-minimal-service-host\cache\kit81\Include\um','-IC:\mt8167-public-fullsource323\wdk81\Include\km','-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration','-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types')
$umd="$root\drivers\gpu\mt8167\wddm-render\umd";$records=@();$closure=@()
# Compiler-generated include closure, rather than a guessed fixed TU count.
foreach($p in Get-ChildItem $umd -Filter '*.c'){
 $name='umd_'+$p.BaseName+'.obj';$f=@($flags)
 if($p.BaseName -eq 'typed_entry'){$f+=@('-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1')}
 & clang @f -M -MT $name -MF "$out\deps\$name.d" $p.FullName;if($LASTEXITCODE){throw "Dependency $name"}
 $affected=(Get-Content "$out\deps\$name.d" -Raw) -match 'typed_device\.h'
 $closure+=@{source=$p.Name;affected=$affected;dependency_sha256=(Sha "$out\deps\$name.d")}
 if(!$affected){continue}
 if($name -notin $members -and $p.BaseName -ne 'typed_entry'){throw "Affected object outside pinned archive $name"}
 & clang @f -c $p.FullName -o "$out\objects\$name";if($LASTEXITCODE){throw "Compile $name"}
 $records+=@{source='drivers/gpu/mt8167/wddm-render/umd/'+$p.Name;source_sha256=(Sha $p.FullName);object=$name;sha256=(Sha "$out\objects\$name");arguments=$f}
}
$changed=@($records|ForEach-Object{$_.object});$rows=@()
foreach($r in $archive[0].members){
 if($r.name -notin $changed){Pin "$out\objects\$($r.name)" $r.sha256}
 $rows+=@{name=$r.name;before_sha256=$r.sha256;sha256=(Sha "$out\objects\$($r.name)");recompiled=($r.name -in $changed)}
}
$paths=@($members|ForEach-Object{"$out\objects\$_"});[array]::Reverse($paths)
& llvm-lib /machine:arm "/out:$out\ge8300-native-umd.lib" @paths;if($LASTEXITCODE){throw 'Archive'}
Copy-Item "$cache\baseline\umd\umd_entry.obj" "$out\objects\umd_entry.obj"
$libs=@();$cm=Get-Content "$cache\compiler\link-manifest.json" -Raw|ConvertFrom-Json
foreach($r in $cm.inputs){
 if($r.role -eq 'verified standard UMD module'){continue}
 $name=[IO.Path]::GetFileName($r.path)
 if($name -eq 'ge8300-pco-arm32.lib'){$lib="$cache\baseline\$name"}else{$lib="$cache\compiler\$name";Pin $lib $r.sha256}
 $libs+=$lib
}
if($libs.Count -ne 10){throw 'Compiler closure10'}
& lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\objects\umd_entry.obj" "$out\objects\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" "$cache\pair\pco_pair.obj" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
if($LASTEXITCODE){throw 'UMD link'}
# Host test runs production callback code; its OS callbacks are mocks, not GPU proof.
$hostflags=@($flags|Where-Object{$_ -notmatch '^--target=|^-D_ARM_$|^-DARM$|^-mno-unaligned-access$'})+@('--target=x86_64-pc-windows-msvc','-D_AMD64_','-DAMD64')
foreach($unit in @(@{path="$root\tests\ge8300_dxgi_present_test.c";name='test'},@{path="$umd\typed_dxgi.c";name='dispatch'})){
 & clang @hostflags -c $unit.path -o "$out\$($unit.name).obj";if($LASTEXITCODE){throw 'Host compile'}
}
$umlib='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\x64'
& lld-link /machine:x64 /subsystem:console /entry:mainCRTStartup /nodefaultlib "$out\test.obj" "$out\dispatch.obj" "$umlib\kernel32.lib" "/out:$out\present-test.exe";if($LASTEXITCODE){throw 'Host test link'}
& "$out\present-test.exe";if($LASTEXITCODE){throw "Host production Present test line $LASTEXITCODE"}
$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 BCBF6C456C6F0DD0818434763B189488FBF13570 "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'Sign'}
& $sign verify /pa "$out\ge8300-umd.dll";if($LASTEXITCODE){throw 'Signature verify'}
[ordered]@{schema='present355-umd-runtime-boundary-v1';compiled_units=$records;include_closure=$closure;archive_members=$rows;retained_link_inputs=@($libs|ForEach-Object{@{name=[IO.Path]::GetFileName($_);sha256=(Sha $_)}});dll_sha256=(Sha "$out\ge8300-umd.dll");production_callback_host_test_exit=0;SDK_rebuilt=$false;compiler_rebuilt=$false;kernel_rebuilt=$false;firmware_rebuilt=$false;client_rebuilt=$false;hardware_verified=$false;display_caps_enabled=$false;primary_scanout_connected=$false;installable_package=$false}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\build-manifest.json"
Compress-Archive "$out\*" "$TaskRoot\outputs.zip"
'PRESENT355_RUNTIME_BOUNDARY_BUILD_COMPLETE'
