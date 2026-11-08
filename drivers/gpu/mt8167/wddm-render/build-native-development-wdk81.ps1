# SPDX-License-Identifier: MIT
# UMD/client only. Never build or install a kernel driver.
param([switch]$EnableCandidate,[switch]$EnableLimitedDevelopmentProfile,[switch]$SourceCompiler,
 [Parameter(Mandatory=$true)][string]$CompilerDirectory,
 [Parameter(Mandatory=$true)][string]$CompilerManifestSha256,
 [Parameter(Mandatory=$true)][string]$PairDirectory,
 [Parameter(Mandatory=$true)][string]$PairManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate -or !$EnableLimitedDevelopmentProfile){throw 'Explicit limited development opt-in required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify($path,$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $sha){throw "Input hash mismatch: $path"}}
$cp=Join-Path $CompilerDirectory 'link-manifest.json';Verify $cp $CompilerManifestSha256
$cm=Get-Content -Raw $cp|ConvertFrom-Json
if($SourceCompiler){
 if($cm.schema -cne 'compiler-source-closure-v1' -or $cm.source_dirty -or !$cm.source_built -or !$cm.all_compile_exit_codes_zero -or $cm.source_commit -cne (& git -C $root rev-parse HEAD) -or $cm.compiled_units -ne 375 -or $cm.source_plan_sha256 -cne (Get-FileHash "$root\third_party\mesa-pco\source-plan.json" -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Source-built compiler closure mismatch'}
}else{if($cm.source_dirty -or $cm.exit_code -ne 0 -or $cm.crt_entry -ne '_DllMainCRTStartup'){throw 'Incomplete compiler/CRT provenance'}}
$libs=@()
foreach($l in $cm.inputs){
 if($l.role -eq 'verified standard UMD module'){continue}
 $path=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($l.path));Verify $path $l.sha256;$libs+=$path
}
if($libs.Count -ne 10){throw 'Incomplete compiler closure'}
$pp=Join-Path $PairDirectory $(if($SourceCompiler){'pair-manifest.json'}else{'manifest.json'});Verify $pp $PairManifestSha256
$pair=Get-Content -Raw $pp|ConvertFrom-Json
if($pair.source_dirty -or !$pair.user_mode_fp -or $pair.kernel_no_fp_applied){throw 'Invalid ARM pair provenance'}
Verify (Join-Path $PairDirectory 'pco_pair.obj') $pair.object_sha256
foreach($prop in $pair.dependencies.PSObject.Properties){
 $name=$prop.Name -replace '\\','/';$mark='drivers/gpu/mt8167/wddm-render/';$at=$name.IndexOf($mark)
 if($at -ge 0){Verify (Join-Path $root $name.Substring($at)) $prop.Value}
}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include";$lib="${env:ProgramFiles(x86)}\Windows Kits\8.1\Lib\winv6.3\um\arm"
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002',"-I$inc\shared","-I$inc\um","-I$inc\km",'-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration')
 $units=@('clear_protocol','frontend/draw_protocol','frontend/draw_link_protocol','frontend/texture_draw_protocol','frontend/shader_draw_protocol','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','umd/typed_state','umd/typed_texture','umd/typed_resource','umd/typed_view','umd/typed_shader','umd/typed_draw','umd/typed_pair','umd/typed_runtime','umd/typed_dxgi','umd/typed_entry','umd/entry','testclient/memory','testclient/d3d-clear','testclient/d3d-texture')
 $objects=@();$compiled=@()
 foreach($u in $units){
  $flags=@($common);$obj=($u -replace '/','_')+'.obj'
  if($u -ne 'umd/typed_state' -and !$u.StartsWith('testclient/')){$flags+='-D_D3D10_CONSTANTS';$flags+='-D_D3D10_1_CONSTANTS'}
  if($u -eq 'umd/typed_entry'){$flags+='-DGEO_TYPED_LIMITED_DEVELOPMENT=1';$flags+='-DGEO_TYPED_NATIVE_PIPELINE10=1'}
  if($u -eq 'umd/entry'){$flags+='-DGEO_LINKED_REAL_FRONTEND=1'}
  if($u -eq 'testclient/d3d-clear'){$flags+='-DGEO_COLOR_PAIR_CLIENT=1';$flags+='-DGEO_NATIVE_PIPELINE10_CLIENT=1'}
  & clang @flags -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c (Join-Path $PSScriptRoot ($u+'.c')) -o (Join-Path $out $obj)
  if($LASTEXITCODE){throw "ARM32 compile failed: $u"}
  $compiled+=@{unit=$u;source_sha256=(Get-FileHash (Join-Path $PSScriptRoot ($u+'.c')) -Algorithm SHA256).Hash;object_sha256=(Get-FileHash (Join-Path $out $obj) -Algorithm SHA256).Hash}
  if($u -notin @('umd/typed_entry','umd/entry','testclient/memory','testclient/d3d-clear','testclient/d3d-texture')){$objects+=Join-Path $out $obj}
 }
 & llvm-lib /machine:arm "/out:$out\ge8300-native-umd.lib" @objects
 if($LASTEXITCODE){throw 'UMD archive failed'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" (Join-Path $PairDirectory 'pco_pair.obj') @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'Native UMD link failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d-clear.obj" "$out\testclient_memory.obj" "$lib\kernel32.lib" "$lib\setupapi.lib" "$lib\dxgi.lib" "$lib\d3d11.lib" "/out:$out\ge8300-d3d-clear.exe"
 if($LASTEXITCODE){throw 'Native client link failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d-texture.obj" "$out\testclient_memory.obj" "$lib\kernel32.lib" "$lib\setupapi.lib" "$lib\dxgi.lib" "$lib\d3d11.lib" "/out:$out\ge8300-d3d-texture.exe"
 if($LASTEXITCODE){throw 'Standard texture client link failed'}
 foreach($name in @('ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe')){
  $pe=@(& llvm-readobj --file-headers --coff-exports --coff-imports "$out\$name")
  if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Not ARMNT'}
  if($name -eq 'ge8300-umd.dll' -and !($pe -match 'Name: OpenAdapter10_2')){throw 'Missing runtime export'}
  $pe|Set-Content "$out\$name.pe.txt"
 }
 @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;compiled=$compiled;compiler_manifest_sha256=$CompilerManifestSha256;pair_manifest_sha256=$PairManifestSha256;default_off=$true;limited_development_profile=$true;development_negotiation_pipeline='10_0';standard_profile_qualified=$false;runtime_verified=$false;kernel_changed=$false;private_payload=$false;signed=$false;outputs=@('ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-native-umd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\$_" -Algorithm SHA256).Hash}}}|ConvertTo-Json -Depth 8|Set-Content "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
