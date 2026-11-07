# SPDX-License-Identifier: MIT
# Incremental typed WinBlue closure. No installation, signing or hardware access.
param([switch]$EnableCandidate,[switch]$EnableLimitedDevelopmentProfile,
 [Parameter(Mandatory=$true)][string]$ModuleDirectory,
 [Parameter(Mandatory=$true)][string]$ModuleManifestSha256,
 [Parameter(Mandatory=$true)][string]$StateDirectory,
 [Parameter(Mandatory=$true)][string]$StateManifestSha256,
 [Parameter(Mandatory=$true)][string]$CompilerDirectory,
 [Parameter(Mandatory=$true)][string]$CompilerManifestSha256,
 [Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,
 [Parameter(Mandatory=$true)][string]$NativeOwnerSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Typed UMD candidate is default off'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Fixed clean source required'}
function Verify-Hash([string]$path,[string]$hash){if($hash -notmatch '^[0-9a-fA-F]{64}$' -or !(Test-Path $path) -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $hash){throw "Fixed input hash mismatch: $path"}}
$mp=Join-Path $ModuleDirectory 'build-manifest.json';Verify-Hash $mp $ModuleManifestSha256
$m=Get-Content -Raw $mp|ConvertFrom-Json
if($m.source_dirty){throw 'Dirty module baseline'}
foreach($s in $m.sources){
 $p=$s.path -replace '\\','/'
 if([IO.Path]::GetExtension($p) -notin @('.c','.h') -or $p -in @('drivers/gpu/mt8167/wddm-render/umd/tokenized_shader.c','drivers/gpu/mt8167/wddm-render/frontend/draw_protocol.c') -or $p.StartsWith('drivers/gpu/mt8167/wddm-render/testclient/')){continue}
 Verify-Hash (Join-Path $root $p) $s.sha256
}
foreach($name in @('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj')){
 $record=@($m.outputs|Where-Object {$_.name -eq $name});if($record.Count -ne 1){throw 'Missing module output'}
 Verify-Hash (Join-Path $ModuleDirectory $name) $record[0].sha256
}
$sp=Join-Path $StateDirectory 'manifest.json';Verify-Hash $sp $StateManifestSha256
$sm=Get-Content -Raw $sp|ConvertFrom-Json
foreach($r in $sm.files){Verify-Hash (Join-Path $StateDirectory $r.path) $r.sha256}
foreach($p in @('umd/typed_state.c','umd/typed_state.h')){
 $old=(& git -C $root rev-parse ($sm.source_commit+':drivers/gpu/mt8167/wddm-render/'+$p));if($LASTEXITCODE){throw 'Missing original state source commit'}
 $now=(& git -C $root hash-object (Join-Path $PSScriptRoot $p));if($LASTEXITCODE -or $old -ne $now){throw 'State source differs from compiled module'}
}
$cp=Join-Path $CompilerDirectory 'link-manifest.json';Verify-Hash $cp $CompilerManifestSha256
$cm=Get-Content -Raw $cp|ConvertFrom-Json
if($cm.exit_code -ne 0 -or $cm.crt_entry -ne '_DllMainCRTStartup'){throw 'Incomplete actual PCO/CRT closure'}
$libraries=@()
foreach($l in $cm.inputs){
 if($l.role -eq 'verified standard UMD module'){continue}
 $path=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($l.path));Verify-Hash $path $l.sha256;$libraries+=$path
}
if($libraries.Count -ne 10){throw 'Incomplete compiler/CRT dependency list'}
Verify-Hash $NativeOwnerLibrary $NativeOwnerSha256
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh output inside checkout required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 & llvm-ar x "--output=$out" (Join-Path $ModuleDirectory 'ge8300-umd-clear.lib')
 if($LASTEXITCODE){throw 'UMD archive extraction failed'}
 Copy-Item (Join-Path $StateDirectory 'typed_state.obj') $out
 Copy-Item (Join-Path $ModuleDirectory 'umd_entry.obj') $out
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
 # WDK d3d10umddi.h emits C const definitions under this guard. The
 # verified Nash state object owns their single definition; other C TUs
 # must not emit duplicate external definitions. No SDK bytes are patched.
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS','-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002',"-I$inc\shared","-I$inc\um","-I$inc\km")
 if($EnableLimitedDevelopmentProfile){$common+='-DGEO_TYPED_LIMITED_DEVELOPMENT=1'}
 $units=@('frontend/draw_protocol','umd/tokenized_shader','umd/typed_entry','umd/typed_resource','umd/typed_view','umd/typed_shader','umd/typed_draw','umd/typed_runtime','umd/typed_dxgi')
 foreach($unit in $units){
  $name=($unit -replace '/','_')+'.obj';$flags=@($common)
  if($unit -eq 'frontend/draw_protocol'){$flags+='-mfpu=none'}
  & clang @flags -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c (Join-Path $PSScriptRoot ($unit+'.c')) -o (Join-Path $out $name)
  if($LASTEXITCODE){throw "Typed ARM compile failed: $unit"}
 }
 Copy-Item (Join-Path $ModuleDirectory 'ge8300-kmd-clear.lib') "$out\ge8300-typed-kmd.lib"
 & llvm-ar r "$out\ge8300-typed-kmd.lib" "$out\frontend_draw_protocol.obj"
 if($LASTEXITCODE){throw 'KMD framing object replacement failed'}
 $names=@('clear_protocol','frontend_draw_protocol','umd_clear_ddi','umd_bootstrap','umd_frontend','umd_shader_state','umd_tokenized_shader','compiler_sm2','typed_state','umd_typed_resource','umd_typed_view','umd_typed_shader','umd_typed_draw','umd_typed_runtime','umd_typed_dxgi')
 $objects=@($names|ForEach-Object {Join-Path $out ($_+'.obj')})
 & llvm-lib /machine:arm "/out:$out\ge8300-typed-umd.lib" @objects
 if($LASTEXITCODE){throw 'Typed UMD archive failed'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-typed-umd.lib" @libraries "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'Real typed UMD DLL link failed'}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\ge8300-typed-kmd.lib" $NativeOwnerLibrary "$km\displib.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE){throw 'Combined typed KMD SYS link failed'}
 foreach($name in @('ge8300-umd.dll','ge8300-render.sys')){
  $pe=@(& llvm-readobj --file-headers --coff-imports --coff-exports (Join-Path $out $name))
  if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Final image not ARMNT'}
  if($name -eq 'ge8300-umd.dll' -and !($pe -match 'Name: OpenAdapter10_2')){throw 'Typed runtime entry not exported'}
  if($name -eq 'ge8300-render.sys' -and $pe -match '(?i)Name: Wdf'){throw 'Unexpected second KMDF owner'}
  $pe|Set-Content (Join-Path $out ($name+'.pe.txt'))
 }
 @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;compile_units=$units;reused_source_commit=$m.source_commit;
  module_manifest_sha256=$ModuleManifestSha256;state_source_commit=$sm.source_commit;state_manifest_sha256=$StateManifestSha256;compiler_manifest_sha256=$CompilerManifestSha256;
  native_owner_sha256=$NativeOwnerSha256;default_off=$true;limited_development_profile=[bool]$EnableLimitedDevelopmentProfile;development_negotiation_pipeline='9_1';standard_profile_qualified=$false;runtime_verified=$false;signed=$false;
  outputs=@('ge8300-typed-kmd.lib','ge8300-typed-umd.lib','ge8300-umd.dll','ge8300-render.sys')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 6|Set-Content "$out\build-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
