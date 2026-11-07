# SPDX-License-Identifier: MIT
# Relink the verified 3.4 objects. No compiler/UMD/firmware rebuild or device IO.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$KernelDirectory,
 [Parameter(Mandatory=$true)][string]$BaselinePackage,
 [Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit kernel link update required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
& git -C $root diff --quiet 0406393650f68c6c4d704f4e6e40e58225d5b8d3 HEAD -- drivers/gpu/mt8167/wddm-render/miniport.c drivers/gpu/mt8167/wddm-render/init_checkpoint.h
if($LASTEXITCODE){throw 'Kernel source changed: compile new objects instead of reusing 3.4'}
function Verify($p,$h){if((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne $h){throw 'Verified link input mismatch'}}
Verify "$KernelDirectory\linked-manifest.json" '45401e11bf2f45651352256cf4ceb529659ded1e24851875e11192766ae4fffc'
Verify "$KernelDirectory\ge8300-pair-kmd.lib" 'be0c72e9aef41e640df98ae231a4041d7f83a147b75f941593d199b097486f74'
Verify $NativeOwnerLibrary 'cee101f279430ff214e853d1104017941fb4df884e75d0e0a764c0eb22b54a87'
Verify "$BaselinePackage\ge8300-umd.dll" 'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'
Verify "$BaselinePackage\ge8300-mips.fw" '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory "$out\linked"|Out-Null
Copy-Item "$KernelDirectory\ge8300-pair-kmd.lib" "$out\linked\ge8300-pair-kmd.lib"
Copy-Item "$BaselinePackage\ge8300-umd.dll" "$out\linked\ge8300-umd.dll"
& "$PSScriptRoot\link-kmd-msvc-wdk81.ps1" -KernelLibrary "$out\linked\ge8300-pair-kmd.lib" -NativeLibrary $NativeOwnerLibrary -OutputFile "$out\linked\ge8300-render.sys" -MapFile "$out\linked\ge8300-render.map" -MetadataFile "$out\linked\linker.json"
$m=Get-Content -Raw "$KernelDirectory\linked-manifest.json"|ConvertFrom-Json
$m.source_commit=(& git -C $root rev-parse HEAD);$m.compilation_performed=$false;$m.additional_compile_units=@()
$m|Add-Member -NotePropertyName kernel_source_commit -NotePropertyValue '0406393650f68c6c4d704f4e6e40e58225d5b8d3'
$m|Add-Member -NotePropertyName kernel_archive_manifest_sha256 -NotePropertyValue '45401e11bf2f45651352256cf4ceb529659ded1e24851875e11192766ae4fffc'
$m|Add-Member -NotePropertyName linker -NotePropertyValue (Get-Content -Raw "$out\linked\linker.json"|ConvertFrom-Json)
$m.outputs=@('ge8300-render.sys','ge8300-umd.dll','ge8300-pair-kmd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\linked\$_" -Algorithm SHA256).Hash}}
$m|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\linked\linked-manifest.json"
& "$PSScriptRoot\package-linked-wdk81.ps1" -EnableCandidate -PreserveSignedUmd -LinkedDirectory "$out\linked" -LinkedManifestSha256 (Get-FileHash "$out\linked\linked-manifest.json" -Algorithm SHA256).Hash -Firmware "$BaselinePackage\ge8300-mips.fw" -OutputDirectory "$out\package"
Copy-Item "$out\linked\linker.json" "$out\package\linker.json"
