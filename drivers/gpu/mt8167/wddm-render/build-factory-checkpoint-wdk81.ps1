# SPDX-License-Identifier: MIT
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$KernelDirectory,[Parameter(Mandatory=$true)][string]$BaselinePackage,[Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit native factory checkpoint opt-in required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
& git -C $root diff --quiet c63ec476e9bbcef3ec04576a58317df40de93564 HEAD -- drivers/gpu/mt8167/wddm-render/miniport.c drivers/gpu/mt8167/wddm-render/init_checkpoint.h
if($LASTEXITCODE){throw 'Miniport source differs from verified 3.6'}
function Verify($p,$h){if((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne $h){throw 'Pinned build input mismatch'}}
Verify "$KernelDirectory\ge8300-pair-kmd.lib" 'f9850487f2b634a23f6321f5e8689091630d3835878cc1bb3aa9a2d49e5817ac'
Verify "$BaselinePackage\ge8300-umd.dll" 'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'
Verify "$BaselinePackage\ge8300-mips.fw" '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
$native=Get-Content -Raw "$NativeDirectory\native-checkpoint-manifest.json"|ConvertFrom-Json
if($native.source_commit -ne 'fd1f36ff4ef8b869dc4e3e4d6d4d6b365cc67c9e' -or $native.changed_abi -ne $false){throw 'Fixed native checkpoint source required'}
Verify "$NativeDirectory\ge8300-native-owner-core.lib" $native.outputs.sha256
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar) -or (Test-Path $out)){throw 'Fresh source-local output required'}
New-Item -ItemType Directory "$out\linked","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include";$resource=(& clang -print-resource-dir)
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DD3D_UMD_INTERFACE_VERSION=0x4002 -DDXGKDDI_INTERFACE_VERSION=0x4002 -DGEO_LINKED_NATIVE_CORE=1 -D_KERNEL_MODE -D__ldrexd=__geo_wdk81_ldrexd_declaration "-I$resource\include" "-I$inc\km" "-I$inc\km\crt" "-I$inc\shared" -c "$PSScriptRoot\miniport.c" -o "$out\miniport.obj"
 if($LASTEXITCODE){throw 'Miniport compile failed'}
 Copy-Item "$KernelDirectory\ge8300-pair-kmd.lib" "$out\linked\ge8300-pair-kmd.lib"
 & llvm-ar r "$out\linked\ge8300-pair-kmd.lib" "$out\miniport.obj";if($LASTEXITCODE){throw 'Miniport archive replacement failed'}
 & "$PSScriptRoot\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$out\linked\ge8300-pair-kmd.lib" -NativeLibrary "$NativeDirectory\ge8300-native-owner-core.lib" -OutputFile "$out\linked\ge8300-render.sys" -MapFile "$out\linked\ge8300-render.map" -MetadataFile "$out\linked\linker.json"
 Copy-Item "$BaselinePackage\ge8300-umd.dll" "$out\linked\ge8300-umd.dll"
 $m=Get-Content -Raw "$BaselinePackage\linked-manifest.json"|ConvertFrom-Json
 $m.source_commit=(& git -C $root rev-parse HEAD);$m.native_source_commit=$native.source_commit;$m.native_owner_sha256=$native.outputs.sha256;$m.additional_compile_units=@('miniport','native_owner_factory','native_owner_factory_platform','native_owner_allocation')
 $m.linker=Get-Content -Raw "$out\linked\linker.json"|ConvertFrom-Json
 $m|Add-Member -NotePropertyName native_checkpoint -NotePropertyValue @{schema=2;record_bytes=40;manifest_sha256=(Get-FileHash "$NativeDirectory\native-checkpoint-manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant()}
 $m.outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-pair-kmd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\linked\$_" -Algorithm SHA256).Hash}}
 $m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\linked\linked-manifest.json"
 & "$PSScriptRoot\package-linked-wdk81.ps1" -EnableCandidate -PreserveSignedUmd -LinkedDirectory "$out\linked" -LinkedManifestSha256 (Get-FileHash "$out\linked\linked-manifest.json" -Algorithm SHA256).Hash -Firmware "$BaselinePackage\ge8300-mips.fw" -OutputDirectory "$out\package"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
