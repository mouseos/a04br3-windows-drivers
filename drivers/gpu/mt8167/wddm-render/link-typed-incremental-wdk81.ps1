# SPDX-License-Identifier: MIT
# Fixed incremental closure: four reviewed TU objects plus ClearView, no GPU access.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$ContractDirectory,
 [Parameter(Mandatory=$true)][string]$ContractManifestSha256,
 [Parameter(Mandatory=$true)][string]$CompilerDirectory,
 [Parameter(Mandatory=$true)][string]$CompilerManifestSha256,
 [Parameter(Mandatory=$true)][string]$NativeCombinedDirectory,
 [Parameter(Mandatory=$true)][string]$NativeCombinedManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Limited development candidate is default off'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify([string]$p,[string]$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $p -Algorithm SHA256).Hash -ne $sha){throw "Hash mismatch: $p"}}
$bp=Join-Path $BaselineDirectory 'build-manifest.json';Verify $bp $BaselineManifestSha256;$b=Get-Content -Raw $bp|ConvertFrom-Json
$cp=Join-Path $ContractDirectory 'manifest.json';Verify $cp $ContractManifestSha256;$c=Get-Content -Raw $cp|ConvertFrom-Json
$np=Join-Path $NativeCombinedDirectory 'combined-manifest.json';Verify $np $NativeCombinedManifestSha256;$n=Get-Content -Raw $np|ConvertFrom-Json
$pp=Join-Path $CompilerDirectory 'link-manifest.json';Verify $pp $CompilerManifestSha256;$compiler=Get-Content -Raw $pp|ConvertFrom-Json
if(!$b.limited_development_profile -or $c.source_dirty -or $n.signed -or $n.runtime_verified -or !$n.limited_development_profile){throw 'Unexpected input qualification/profile'}
# The sole shared header change appends an adapter trace handle. All TUs which
# allocate an adapter or access it beyond its original fields are reviewed here.
$render='drivers/gpu/mt8167/wddm-render/'
$allowed=@('umd/typed_entry.c','umd/typed_resource.c','umd/typed_view.c','umd/typed_shader.c','umd/typed_draw.c','umd/typed_device.h')
$diff=@(& git -C $root diff --name-only $b.source_commit HEAD -- ($render+'*.c') ($render+'*.h'))
if($LASTEXITCODE){throw 'Baseline diff failed'}
foreach($p in $diff){if($p.Substring($render.Length) -notin $allowed){throw "Unrecompiled runtime source: $p"}}
foreach($s in $c.sources){Verify (Join-Path $root $s.path) $s.sha256}
foreach($f in $c.files){Verify (Join-Path $ContractDirectory $f.path) $f.sha256}
Verify (Join-Path $NativeCombinedDirectory 'ge8300-render.sys') $n.sys_sha256
$kmd=@($b.outputs|Where-Object {$_.name -eq 'ge8300-typed-kmd.lib'})
if($kmd.Count -ne 1 -or $n.frontend_kmd_sha256 -ne $kmd[0].sha256){throw 'Native combined SYS uses a different KMD'}
Verify (Join-Path $BaselineDirectory 'ge8300-typed-kmd.lib') $kmd[0].sha256
$libs=@();foreach($l in $compiler.inputs){if($l.role -eq 'verified standard UMD module'){continue};$p=Join-Path $CompilerDirectory ([IO.Path]::GetFileName($l.path));Verify $p $l.sha256;$libs+=$p}
if($libs.Count -ne 10 -or $compiler.exit_code -ne 0){throw 'Incomplete PCO/CRT closure'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $reused=@()
 foreach($r in $b.reused_objects){$p=Join-Path $BaselineDirectory $r.name;Verify $p $r.sha256;Copy-Item $p $out;$reused+=$r}
 foreach($unit in $c.units){Copy-Item (Join-Path $ContractDirectory ('umd_'+$unit+'.obj')) $out -Force}
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $flags=@($c.flags)
 & clang @flags -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c (Join-Path $PSScriptRoot 'umd/typed_view.c') -o "$out\umd_typed_view.obj"
 if($LASTEXITCODE){throw 'ClearView ARM32 compilation failed'}
 $names=@('clear_protocol','frontend_draw_protocol','umd_clear_ddi','umd_bootstrap','umd_frontend','umd_shader_state','umd_tokenized_shader','compiler_sm2','typed_state','umd_typed_resource','umd_typed_view','umd_typed_shader','umd_typed_draw','umd_typed_runtime','umd_typed_dxgi')
 $objects=@($names|ForEach-Object {Join-Path $out ($_+'.obj')})
 & llvm-lib /machine:arm "/out:$out\ge8300-typed-umd.lib" @objects
 if($LASTEXITCODE){throw 'UMD archive failed'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$out\umd_entry.obj" "$out\umd_typed_entry.obj" "$out\ge8300-typed-umd.lib" @libs "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE){throw 'Final typed UMD link failed'}
 Copy-Item (Join-Path $NativeCombinedDirectory 'ge8300-render.sys') $out
 foreach($name in @('ge8300-umd.dll','ge8300-render.sys')){
  $pe=@(& llvm-readobj --file-headers --coff-imports --coff-exports (Join-Path $out $name))
  if($LASTEXITCODE -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Not ARMNT'}
  if($name -eq 'ge8300-umd.dll' -and !($pe -match 'Name: OpenAdapter10_2')){throw 'Missing typed entry'}
  if($name -eq 'ge8300-render.sys' -and $pe -match '(?i)Name: Wdf'){throw 'Unexpected KMDF IRQ owner'}
  $pe|Set-Content (Join-Path $out ($name+'.pe.txt'))
 }
 @{source_commit=(& git -C $root rev-parse HEAD);baseline_source_commit=$b.source_commit;contract_source_commit=$c.source_commit;native_source_commit=$n.native_source_commit;
  baseline_manifest_sha256=$BaselineManifestSha256;contract_manifest_sha256=$ContractManifestSha256;compiler_manifest_sha256=$CompilerManifestSha256;native_combined_manifest_sha256=$NativeCombinedManifestSha256;
  native_owner_sha256=$n.native_owner_sha256;required_parent_commit=$n.parent_source;required_acpi_commit=$n.acpi_source;parent_profile=$n.parent_profile;
  compilation_performed=$false;additional_compile_units=@('umd/typed_view.c');reused_objects=$reused;default_off=$true;limited_development_profile=$true;runtime_verified=$false;standard_profile_qualified=$false;signed=$false;installed=$false;
  outputs=@('ge8300-umd.dll','ge8300-render.sys','ge8300-typed-umd.lib')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 8|Set-Content "$out\linked-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
