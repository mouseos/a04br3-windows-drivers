# SPDX-License-Identifier: MIT
# Rebuild the reviewed UMD operation traces and descriptor validator only.
# Compiler, native KMD, shaders, and signed client remain fixed inputs.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$CompilerDirectory,
 [Parameter(Mandatory=$true)][string]$CompilerManifestSha256,
 [Parameter(Mandatory=$true)][string]$PairDirectory,
 [Parameter(Mandatory=$true)][string]$PairManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit development opt-in required'}
$root=((& git -C $PSScriptRoot rev-parse --show-toplevel)|Select-Object -First 1).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify($p,$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $p -Algorithm SHA256).Hash -ne $sha){throw "Input mismatch: $p"}}
$bp=Join-Path $BaselineDirectory 'build-manifest.json';Verify $bp $BaselineManifestSha256;$b=Get-Content -Raw $bp|ConvertFrom-Json
if($b.source_commit -ne 'f4f1b4f94ade6f59efef042f680c2c3e363442f8' -or $b.source_dirty -or !$b.limited_development_profile -or $b.development_negotiation_pipeline -ne '10_0'){throw 'Fixed native development baseline required'}
$component='drivers/gpu/mt8167/wddm-render/'
$units=@('clear_protocol','umd/typed_entry','umd/typed_resource','umd/typed_view','umd/typed_draw','umd/typed_pair')
$reviewed=@($units|ForEach-Object {$component+$_+'.c'})+@($component+'clear_protocol.h')
foreach($p in @(& git -C $root diff --name-only $b.source_commit HEAD -- ($component+'umd') ($component+'compiler') ($component+'frontend') ($component+'clear_protocol.c') ($component+'clear_protocol.h') ($component+'memory.h') ($component+'adapter_info.h'))){if($p -notin $reviewed){throw 'Unreviewed UMD dependency change'}}
foreach($name in @('ge8300-native-umd.lib')){$record=@($b.outputs|Where-Object {$_.name -eq $name});if($record.Count -ne 1){throw 'Missing baseline archive'};Verify (Join-Path $BaselineDirectory $name) $record[0].sha256}
$entry=@($b.compiled|Where-Object {$_.unit -eq 'umd/entry'});if($entry.Count -ne 1){throw 'Missing export object provenance'}
Verify (Join-Path $BaselineDirectory 'umd_entry.obj') $entry[0].object_sha256
$cp=Join-Path $CompilerDirectory 'link-manifest.json';Verify $cp $CompilerManifestSha256;$cm=Get-Content -Raw $cp|ConvertFrom-Json
if($cm.exit_code -ne 0 -or $cm.source_dirty -or $cm.crt_entry -ne '_DllMainCRTStartup'){throw 'Invalid compiler closure'}
$libs=@();foreach($l in $cm.inputs){if($l.role -eq 'verified standard UMD module'){continue};$p=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($l.path));Verify $p $l.sha256;$libs+=$p}
if($libs.Count -ne 10){throw 'Incomplete compiler/CRT closure'}
$pp=Join-Path $PairDirectory 'manifest.json';Verify $pp $PairManifestSha256;$pair=Get-Content -Raw $pp|ConvertFrom-Json
Verify (Join-Path $PairDirectory 'pco_pair.obj') $pair.object_sha256
if($pair.source_dirty -or !$pair.user_mode_fp -or $pair.kernel_no_fp_applied){throw 'Invalid ARM pair provenance'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 Copy-Item "$BaselineDirectory\ge8300-native-umd.lib" $out;Copy-Item "$BaselineDirectory\umd_entry.obj" $out
 $inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
 $flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002','-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS','-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration',"-I$inc\shared","-I$inc\um","-I$inc\km")
 $compiled=@()
 foreach($unit in $units){
  $extra=@();if($unit -eq 'umd/typed_entry'){$extra=@('-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1')}
  $object=($unit -replace '/','_')+'.obj'
  & clang @flags @extra -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c "$PSScriptRoot\$unit.c" -o "$out\$object"
  if($LASTEXITCODE){throw "ARM UMD operation trace compile failed: $unit"}
  $compiled+=@{unit=$unit;source_sha256=(Get-FileHash "$PSScriptRoot\$unit.c" -Algorithm SHA256).Hash;object_sha256=(Get-FileHash "$out\$object" -Algorithm SHA256).Hash}
 }
 $before=@(& llvm-ar t "$out\ge8300-native-umd.lib")
 foreach($unit in @($units|Where-Object {$_ -ne 'umd/typed_entry'})){
  $object=($unit -replace '/','_')+'.obj'
  if(@($before|Where-Object {[IO.Path]::GetFileName($_) -eq $object}).Count -ne 1){throw "Unique archive member required: $object"}
  & llvm-ar r "$out\ge8300-native-umd.lib" "$out\$object"
  if($LASTEXITCODE){throw 'Archive replacement failed'}
 }
 $after=@(& llvm-ar t "$out\ge8300-native-umd.lib")
 if($after.Count -ne $before.Count){throw 'Archive member count changed'}
 foreach($unit in @($units|Where-Object {$_ -ne 'umd/typed_entry'})){
  $object=($unit -replace '/','_')+'.obj'
  if(@($after|Where-Object {[IO.Path]::GetFileName($_) -eq $object}).Count -ne 1){throw "Unique archive replacement failed: $object"}
 }
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-native-umd.lib" (Join-Path $PairDirectory 'pco_pair.obj') @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'UMD link failed'}
 @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;baseline_source_commit=$b.source_commit;baseline_manifest_sha256=$BaselineManifestSha256;compiler_manifest_sha256=$CompilerManifestSha256;pair_manifest_sha256=$PairManifestSha256;compiled=$compiled;limited_development_profile=$true;development_negotiation_pipeline='10_0';standard_profile_qualified=$false;runtime_verified=$false;kernel_changed=$false;client_changed=$false;outputs=@('ge8300-native-umd.lib','ge8300-umd.dll')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
