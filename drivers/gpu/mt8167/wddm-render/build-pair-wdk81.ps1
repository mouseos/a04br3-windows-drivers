# SPDX-License-Identifier: MIT
# Limited COLOR pair closure; reuse actual flat objects/PCO/MDL-SEH unchanged.
param([switch]$EnableCandidate,[switch]$EnableColorPair,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$KmdDirectory,
 [Parameter(Mandatory=$true)][string]$KmdManifestSha256,
 [Parameter(Mandatory=$true)][string]$CompilerDirectory,
 [Parameter(Mandatory=$true)][string]$CompilerManifestSha256,
 [Parameter(Mandatory=$true)][string]$PairDirectory,
 [Parameter(Mandatory=$true)][string]$PairManifestSha256,
 [Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,
 [Parameter(Mandatory=$true)][string]$NativeOwnerSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate -or !$EnableColorPair){throw 'COLOR limited development candidate is default off'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify([string]$p,[string]$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $p -Algorithm SHA256).Hash -ne $sha){throw "Hash mismatch: $p"}}
$bp=Join-Path $BaselineDirectory 'linked-manifest.json';Verify $bp $BaselineManifestSha256;$b=Get-Content -Raw $bp|ConvertFrom-Json
$kp=Join-Path $KmdDirectory 'build-manifest.json';Verify $kp $KmdManifestSha256;$k=Get-Content -Raw $kp|ConvertFrom-Json
$cp=Join-Path $CompilerDirectory 'link-manifest.json';Verify $cp $CompilerManifestSha256;$cm=Get-Content -Raw $cp|ConvertFrom-Json
$pp=Join-Path $PairDirectory 'manifest.json';Verify $pp $PairManifestSha256;$pair=Get-Content -Raw $pp|ConvertFrom-Json
if($pair.source_dirty -or !$pair.user_mode_fp -or $pair.kernel_no_fp_applied -or $pair.source_commit -notmatch '^[a-f0-9]{40}$'){throw 'Expected fixed current user-mode ARM32 pair object'}
Verify (Join-Path $PairDirectory 'pco_pair.obj') $pair.object_sha256
# Local compile paths differ from runner checkout. Verify every pair compiler
# source/header under this component against committed git blobs via hashes.
foreach($property in $pair.dependencies.PSObject.Properties){
 $name=$property.Name -replace '\\','/';$mark='drivers/gpu/mt8167/wddm-render/';$at=$name.IndexOf($mark)
 if($at -ge 0){$file=Join-Path $root $name.Substring($at);Verify $file $property.Value}
}
Verify $NativeOwnerLibrary $NativeOwnerSha256
$ur=@($b.outputs|Where-Object {$_.name -eq 'ge8300-typed-umd.lib'});if($ur.Count -ne 1){throw 'Missing flat UMD archive'}
Verify (Join-Path $BaselineDirectory $ur[0].name) $ur[0].sha256
$kr=@($k.outputs|Where-Object {$_.name -eq 'ge8300-typed-kmd.lib'});if($kr.Count -ne 1){throw 'Missing KMD archive'}
Verify (Join-Path $KmdDirectory $kr[0].name) $kr[0].sha256
$libs=@();foreach($l in $cm.inputs){if($l.role -eq 'verified standard UMD module'){continue};$p=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($l.path));Verify $p $l.sha256;$libs+=$p}
if($libs.Count -ne 10 -or $cm.exit_code -ne 0){throw 'Incomplete PCO/CRT closure'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 & llvm-ar x "--output=$out" (Join-Path $BaselineDirectory 'ge8300-typed-umd.lib')
 if($LASTEXITCODE){throw 'UMD extraction failed'}
 $entry=@($k.reused_objects|Where-Object {$_.name -eq 'umd_entry.obj'});if($entry.Count -ne 1){throw 'Missing export object provenance'}
 Verify (Join-Path $KmdDirectory 'umd_entry.obj') $entry[0].sha256;Copy-Item (Join-Path $KmdDirectory 'umd_entry.obj') $out
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm';$resource=(& clang -print-resource-dir)
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002',"-I$inc\shared")
 $kernel=@('capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory')
 Copy-Item (Join-Path $KmdDirectory 'ge8300-typed-kmd.lib') "$out\ge8300-pair-kmd.lib"
 foreach($u in $kernel){
  & clang @common -mfpu=none -DGEO_LINKED_NATIVE_CORE=1 -D_KERNEL_MODE -D__ldrexd=__geo_wdk81_ldrexd_declaration "-I$resource\include" "-I$inc\km" "-I$inc\km\crt" -c (Join-Path $PSScriptRoot ($u+'.c')) -o "$out\$u.obj"
  if($LASTEXITCODE){throw "KMD compile failed: $u"}
  & llvm-ar r "$out\ge8300-pair-kmd.lib" "$out\$u.obj";if($LASTEXITCODE){throw 'KMD object replacement failed'}
 }
 $user=@('umd/tokenized_shader','umd/typed_entry','umd/typed_draw','umd/typed_pair','frontend/draw_link_protocol')
 foreach($u in $user){
  $obj=($u -replace '/','_')+'.obj'
  & clang @common -D_D3D10_CONSTANTS -D_D3D10_1_CONSTANTS -D__ldrexd=__geo_typed_wdk81_ldrexd_declaration -DGEO_TYPED_LIMITED_DEVELOPMENT=1 "-I$inc\um" "-I$inc\km" -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c (Join-Path $PSScriptRoot ($u+'.c')) -o (Join-Path $out $obj)
  if($LASTEXITCODE){throw "UMD compile failed: $u"}
 }
 $names=@('clear_protocol','frontend_draw_protocol','frontend_draw_link_protocol','umd_clear_ddi','umd_bootstrap','umd_frontend','umd_shader_state','umd_tokenized_shader','compiler_sm2','typed_state','umd_typed_resource','umd_typed_view','umd_typed_shader','umd_typed_draw','umd_typed_pair','umd_typed_runtime','umd_typed_dxgi')
 $objects=@($names|ForEach-Object {Join-Path $out ($_+'.obj')})
 & llvm-lib /machine:arm "/out:$out\ge8300-pair-umd.lib" @objects
 if($LASTEXITCODE){throw 'UMD archive failed'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-pair-umd.lib" (Join-Path $PairDirectory 'pco_pair.obj') @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'Final COLOR UMD link failed'}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\ge8300-pair-kmd.lib" $NativeOwnerLibrary "$km\displib.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'Final COLOR KMD link failed'}
 foreach($name in @('ge8300-umd.dll','ge8300-render.sys')){
  $pe=@(& llvm-readobj --file-headers --coff-imports --coff-exports (Join-Path $out $name));if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Not ARMNT'}
  if($name -eq 'ge8300-umd.dll' -and !($pe -match 'Name: OpenAdapter10_2')){throw 'Missing standard entry'}
  if($name -eq 'ge8300-render.sys' -and $pe -match '(?i)Name: Wdf'){throw 'Unexpected KMDF IRQ owner'}
  $pe|Set-Content (Join-Path $out ($name+'.pe.txt'))
 }
 @{source_commit=(& git -C $root rev-parse HEAD);baseline_manifest_sha256=$BaselineManifestSha256;kmd_manifest_sha256=$KmdManifestSha256;compiler_manifest_sha256=$CompilerManifestSha256;pair_manifest_sha256=$PairManifestSha256;native_owner_sha256=$NativeOwnerSha256;
  native_source_commit='e83792d88485883130e137309b0ef027b3c7bc5e';required_parent_commit=$b.required_parent_commit;required_acpi_commit=$b.required_acpi_commit;compilation_performed=$true;additional_compile_units=@($kernel+$user);default_off=$true;limited_development_profile=$true;color_pair_profile=$true;runtime_verified=$false;standard_profile_qualified=$false;signed=$false;installed=$false;
  outputs=@('ge8300-umd.dll','ge8300-render.sys','ge8300-pair-umd.lib','ge8300-pair-kmd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 8|Set-Content "$out\linked-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
