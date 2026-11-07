# SPDX-License-Identifier: MIT
# Rebuild only miniport.obj; preserve the existing compiler, UMD and native owner.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselinePackage,
 [Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit initialization update required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify($path,$hash){if((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash){throw 'Baseline hash mismatch'}}
Verify "$BaselineDirectory\linked-manifest.json" 'f560e326e948587a5458fc75e2b80fd5e7cd3037750ce9d9107b4803eca459b2'
Verify "$BaselinePackage\signed-manifest.json" 'bec42dddb85c79bc8a272d8d8b94a312d9c86e41a4f20c30664ec14cc8945f88'
Verify "$BaselineDirectory\ge8300-pair-kmd.lib" 'f9850487f2b634a23f6321f5e8689091630d3835878cc1bb3aa9a2d49e5817ac'
Verify "$BaselinePackage\ge8300-umd.dll" 'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'
Verify "$BaselinePackage\ge8300-mips.fw" '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
Verify $NativeOwnerLibrary 'cee101f279430ff214e853d1104017941fb4df884e75d0e0a764c0eb22b54a87'
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp","$out\linked"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm"
 $resource=(& clang -print-resource-dir)
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DD3D_UMD_INTERFACE_VERSION=0x4002 -DDXGKDDI_INTERFACE_VERSION=0x4002 -DGEO_LINKED_NATIVE_CORE=1 -D_KERNEL_MODE -D__ldrexd=__geo_wdk81_ldrexd_declaration "-I$resource\include" "-I$inc\km" "-I$inc\km\crt" "-I$inc\shared" -c "$PSScriptRoot\miniport.c" -o "$out\miniport.obj"
 if($LASTEXITCODE){throw 'Initialization miniport compilation failed'}
 Copy-Item "$BaselineDirectory\ge8300-pair-kmd.lib" "$out\linked\ge8300-pair-kmd.lib"
 $members=@(& llvm-ar t "$out\linked\ge8300-pair-kmd.lib")
 if($LASTEXITCODE -or @($members|Where-Object {$_ -match '(^|[/\\])miniport\.obj$'}).Count -ne 1){throw 'Expected exactly one old miniport object'}
 & llvm-ar r "$out\linked\ge8300-pair-kmd.lib" "$out\miniport.obj";if($LASTEXITCODE){throw 'Object replacement failed'}
 & "$PSScriptRoot\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$out\linked\ge8300-pair-kmd.lib" -NativeLibrary $NativeOwnerLibrary -OutputFile "$out\linked\ge8300-render.sys" -MapFile "$out\linked\ge8300-render.map" -MetadataFile "$out\linked\linker.json"
 if($LASTEXITCODE){throw 'Updated miniport link failed'}
 Copy-Item "$BaselinePackage\ge8300-umd.dll" "$out\linked\ge8300-umd.dll"
 $m=Get-Content -Raw "$BaselineDirectory\linked-manifest.json"|ConvertFrom-Json
 $m.source_commit=(& git -C $root rev-parse HEAD);$m.additional_compile_units=@('miniport');$m.outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-pair-kmd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\linked\$_" -Algorithm SHA256).Hash}}
 $m|Add-Member -NotePropertyName linker -NotePropertyValue (Get-Content -Raw "$out\linked\linker.json"|ConvertFrom-Json)
 $m|Add-Member -NotePropertyName initialization_checkpoint -NotePropertyValue 'fixed Services/GE8300Render/Parameters/NativeCheckpoint; real stage statuses'
 $m|Add-Member -NotePropertyName preserved_signed_umd_sha256 -NotePropertyValue 'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'
 $m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\linked\linked-manifest.json"
 $hash=(Get-FileHash "$out\linked\linked-manifest.json" -Algorithm SHA256).Hash
 & "$PSScriptRoot\package-linked-wdk81.ps1" -EnableCandidate -PreserveSignedUmd -LinkedDirectory "$out\linked" -LinkedManifestSha256 $hash -Firmware "$BaselinePackage\ge8300-mips.fw" -OutputDirectory "$out\package"
 Verify "$out\package\ge8300-umd.dll" 'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
