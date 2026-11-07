# SPDX-License-Identifier: MIT
# One changed frontend TU; the exact KMD, entry and other UMD objects are reused.
param([Parameter(Mandatory=$true)][string]$VerifiedModuleDirectory,
 [Parameter(Mandatory=$true)][string]$ModuleManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Fixed clean source required'}
$mp=Join-Path $VerifiedModuleDirectory 'build-manifest.json'
if($ModuleManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash $mp -Algorithm SHA256).Hash -ne $ModuleManifestSha256){throw 'Fixed baseline manifest mismatch'}
$m=Get-Content -Raw $mp|ConvertFrom-Json
if($m.source_dirty){throw 'Dirty baseline'}
foreach($s in $m.sources){
 $p=$s.path -replace '\\','/'
 if([IO.Path]::GetExtension($p) -notin @('.c','.h') -or $p -eq 'drivers/gpu/mt8167/wddm-render/umd/frontend.c' -or $p.StartsWith('drivers/gpu/mt8167/wddm-render/testclient/')){continue}
 if((Get-FileHash (Join-Path $root $p) -Algorithm SHA256).Hash -ne $s.sha256){throw "Unchanged runtime dependency differs: $p"}
}
foreach($name in @('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj')){
 $r=@($m.outputs|Where-Object {$_.name -eq $name})
 if($r.Count -ne 1 -or (Get-FileHash (Join-Path $VerifiedModuleDirectory $name) -Algorithm SHA256).Hash -ne $r[0].sha256){throw 'Module output hash mismatch'}
}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh output inside checkout required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 & llvm-ar x "--output=$out" (Join-Path $VerifiedModuleDirectory 'ge8300-umd-clear.lib')
 if($LASTEXITCODE){throw 'Verified UMD extraction failed'}
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 -DD3D_UMD_INTERFACE_VERSION=0x4002 "-I$inc\shared" "-I$inc\um" -c (Join-Path $PSScriptRoot 'umd\frontend.c') -o "$out\umd_frontend.obj"
 if($LASTEXITCODE){throw 'Actual frontend ARM compile failed'}
 $names=@('clear_protocol','frontend_draw_protocol','umd_clear_ddi','umd_bootstrap','umd_frontend','umd_shader_state','umd_tokenized_shader','compiler_sm2','testclient_memory')
 $objects=@($names|ForEach-Object {Join-Path $out ($_.ToString()+'.obj')})
 & llvm-lib /machine:arm "/out:$out\ge8300-umd-clear.lib" @objects
 if($LASTEXITCODE){throw 'Actual frontend archive failed'}
 Copy-Item (Join-Path $VerifiedModuleDirectory 'ge8300-kmd-clear.lib') $out
 Copy-Item (Join-Path $VerifiedModuleDirectory 'umd_entry.obj') $out
 $sources=@(Get-ChildItem $PSScriptRoot -File -Recurse|Where-Object {$_.Extension -in @('.c','.h')}|ForEach-Object {@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=$false;compile_units=@('umd/frontend');sources=$sources;
  reused_source_commit=$m.source_commit;reused_manifest_sha256=$ModuleManifestSha256;runtime_verified=$false;
  outputs=@('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 6|Set-Content "$out\build-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
