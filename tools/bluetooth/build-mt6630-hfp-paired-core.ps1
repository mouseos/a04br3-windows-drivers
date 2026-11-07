# Compile the fixed Raman owner against this BT core, without editing its source.
# No parent/SYS/INF version, signing, deployment or capability advertisement.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Use a new directory inside this checkout'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'A clean fixed source checkout is required'}
$audioCommit='4b2a461245274728d01c86e2dad833b0fa398b5f'
& git -C $root fetch --no-tags origin $audioCommit
if($LASTEXITCODE){throw 'Fixed Raman source fetch failed'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $archive=Join-Path $out 'audio-source.zip';$dependency=Join-Path $out 'dependency'
 & git -C $root archive --format=zip "--output=$archive" $audioCommit drivers/audio/mt8167-afe drivers/bus/mt8167-shared
 if($LASTEXITCODE){throw 'Fixed Audio source archive failed'}
 Expand-Archive -LiteralPath $archive -DestinationPath $dependency
 $audio=Join-Path $dependency 'drivers\audio\mt8167-afe'
 & "$PSScriptRoot\build-mt6630-hfp-endpoint-core.ps1" -OutputDirectory (Join-Path $out 'bt')
 $btManifest=Get-Content (Join-Path $out 'bt\hfp-endpoint-manifest.json') -Raw|ConvertFrom-Json
 if($btManifest.source_commit -ne $commit -or $btManifest.provider_contract_version -ne 2 -or !$btManifest.wdk_compile_archive_succeeded){throw 'BT core mismatch'}
 $common=@($btManifest.kernel_compile_flags)+@('-fno-builtin','-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_AUDIO_HFP_PROVIDER=1',"-I$root\drivers\bluetooth\mt6630")
 $objects=@();$sources=@()
 foreach($name in @('hfp_memif.c','hfp_provider.cpp','hfp_power.cpp')){
  $source=Join-Path $audio $name;$obj=Join-Path $out ($name+'.obj');$objects+=$obj
  if($name.EndsWith('.cpp')){& clang++ @common -std=c++14 -fno-exceptions -fno-rtti -c $source -o $obj}
  else {& clang @common -std=c11 -c $source -o $obj}
  if($LASTEXITCODE){throw "Paired Audio owner compile failed: $name"}
  $sources+=@{path=('drivers/audio/mt8167-afe/'+$name);source_commit=$audioCommit;sha256=(Get-FileHash $source -Algorithm SHA256).Hash}
 }
 foreach($name in @('sideband.c','hfp_endpoint.c','ks_endpoints.cpp','hfp_topology.cpp','hfp_pnp.cpp')){
  $objects+=Join-Path $out ('bt\'+$name+'.obj')
 }
 $library=Join-Path $out 'mt6630-mt8167-hfp-paired-core.lib'
 & lld-link /lib /machine:arm @objects "/out:$library"
 if($LASTEXITCODE){throw 'Paired 8-TU archive failed'}
 $dependencyHashes=@(Get-ChildItem $dependency -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring($dependency.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 @{bt_source_commit=$commit;audio_source_commit=$audioCommit;source_dirty=$false;provider_contract_version=2;
  sources=$sources;audio_dependency_hashes=$dependencyHashes;kernel_compile_flags=$common;
  bt_manifest_sha256=(Get-FileHash (Join-Path $out 'bt\hfp-endpoint-manifest.json') -Algorithm SHA256).Hash;
  library='mt6630-mt8167-hfp-paired-core.lib';sha256=(Get-FileHash $library -Algorithm SHA256).Hash;
  explicit_object_count=$objects.Count;wdk_compile_archive_succeeded=$true;final_driver_link_verified=$false;
  audio_source_modified=$false;parent_built=$false;parent_flags_modified=$false;
  controller_registration_implemented=$false;independent_dma_drain_implemented=$false;
  hci_bypass_capability_advertised=$false;standard_stack_verified=$false;hardware_verified=$false;
  exclusive_bluetooth=$true;wifi_coexistence_verified=$false
 }|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 (Join-Path $out 'paired-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
