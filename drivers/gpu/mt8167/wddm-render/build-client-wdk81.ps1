# SPDX-License-Identifier: MIT
# Incremental standard client only; never recompile SYS, UMD or PCO.
param([switch]$EnableColorPair,[Parameter(Mandatory=$true)][string]$VerifiedModuleDirectory,
 [Parameter(Mandatory=$true)][string]$ModuleManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed client source required'}
$manifestPath=Join-Path $VerifiedModuleDirectory 'build-manifest.json'
if($ModuleManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash $manifestPath -Algorithm SHA256).Hash -ne $ModuleManifestSha256){throw 'Fixed module provenance mismatch'}
$m=Get-Content -Raw $manifestPath|ConvertFrom-Json
if($m.source_dirty){throw 'Dirty module baseline'}
$memory='drivers/gpu/mt8167/wddm-render/testclient/memory.c'
$record=@($m.sources|Where-Object {($_.path -replace '\\','/') -eq $memory})
if($record.Count -ne 1){throw 'Missing client memory provenance'}
$oldMemory=(& git -C $root rev-parse ($m.source_commit+':'+$memory))
if($LASTEXITCODE){throw 'Missing fixed memory source commit'}
$newMemory=(& git -C $root hash-object (Join-Path $root $memory))
if($LASTEXITCODE -or $newMemory -ne $oldMemory){throw 'Client memory implementation changed'}
$archive=Join-Path $VerifiedModuleDirectory 'ge8300-umd-clear.lib'
$record=@($m.outputs|Where-Object {$_.name -eq 'ge8300-umd-clear.lib'})
if($record.Count -ne 1 -or (Get-FileHash $archive -Algorithm SHA256).Hash -ne $record[0].sha256){throw 'Memory archive hash mismatch'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh output inside checkout required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
 & llvm-ar x "--output=$out" $archive testclient_memory.obj
 if($LASTEXITCODE){throw 'Memory member extraction failed'}
 $source=Join-Path $PSScriptRoot 'testclient\d3d-clear.c'
 $extra=@();if($EnableColorPair){$extra+='-DGEO_COLOR_PAIR_CLIENT=1'}
 & clang @extra --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$inc\shared" "-I$inc\um" "-I$inc\km" -c $source -o "$out\d3d-clear.obj"
 if($LASTEXITCODE){throw 'Standard client compile failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\d3d-clear.obj" "$out\testclient_memory.obj" "$lib\kernel32.lib" "$lib\setupapi.lib" "$lib\dxgi.lib" "$lib\d3d11.lib" "/out:$out\ge8300-d3d-clear.exe"
 if($LASTEXITCODE){throw 'Standard client link failed'}
 $pe=@(& llvm-readobj --file-headers --coff-imports "$out\ge8300-d3d-clear.exe")
 if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Client is not ARMNT'}
 $pe|Set-Content "$out\pe.txt"
 @{source_commit=(& git -C $root rev-parse HEAD);source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;shader_header_sha256=(Get-FileHash (Join-Path $PSScriptRoot 'testclient\fixed_shaders.h') -Algorithm SHA256).Hash;module_source_commit=$m.source_commit;
  memory_archive_sha256=$record[0].sha256;memory_object_sha256=(Get-FileHash "$out\testclient_memory.obj" -Algorithm SHA256).Hash;
  module_manifest_sha256=$ModuleManifestSha256;runtime_verified=$false;color_pair_profile=[bool]$EnableColorPair;standard_api_only=$true;private_payload=$false;
  outputs=@(@{name='ge8300-d3d-clear.exe';sha256=(Get-FileHash "$out\ge8300-d3d-clear.exe" -Algorithm SHA256).Hash})
 }|ConvertTo-Json -Depth 5|Set-Content "$out\build-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
