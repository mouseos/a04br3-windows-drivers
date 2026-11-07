# Compile/archive only the Audio owner/common units. No SYS/INF/signing,
# deployment, PnP registration, runner provisioning or device operations.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){
 throw 'Use a new output directory inside this checkout'
}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'A clean fixed source checkout is required'}
$bt='63c32fdc829a8ba65152a49e30ffdc8a1131ae17'
$priorAction=$ErrorActionPreference
try {$ErrorActionPreference='Continue';& git -C $root cat-file -e ($bt+'^{commit}') 2>$null;$available=$LASTEXITCODE -eq 0}
finally {$ErrorActionPreference=$priorAction}
if(!$available){& git -C $root fetch --no-tags origin $bt;if($LASTEXITCODE){throw 'Pinned BT dependency fetch failed'}}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $archive=Join-Path $out 'bt-dependency.zip';$dependency=Join-Path $out 'dependency'
 & git -C $root archive --format=zip "--output=$archive" $bt drivers/bluetooth/mt6630
 if($LASTEXITCODE){throw 'Pinned dependency archive failed'}
 Expand-Archive -LiteralPath $archive -DestinationPath $dependency
 $headers=Join-Path $dependency 'drivers\bluetooth\mt6630'
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $common=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
  '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions',
  '-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',
  '-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_AUDIO_HFP_PROVIDER=1',
  "-I$inc\km","-I$inc\shared","-I$inc\um","-I$headers")
 $objects=@();$sources=@()
 foreach($name in @('hfp_memif.c','hfp_provider.cpp','hfp_power.cpp')){
  $source=Join-Path $PSScriptRoot $name;$obj=Join-Path $out ($name+'.obj');$objects+=$obj
  if($name.EndsWith('.cpp')){& clang++ @common -std=c++14 -fno-exceptions -fno-rtti -c $source -o $obj}
  else {& clang @common -std=c11 -c $source -o $obj}
  if($LASTEXITCODE){throw "HFP owner module compile failed: $name"}
  $sources+=@{path=('drivers/audio/mt8167-afe/'+$name);sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 $library=Join-Path $out 'mt8167-hfp-owner-common.lib'
 & lld-link /lib /machine:arm @objects "/out:$library"
 if($LASTEXITCODE){throw 'HFP owner/common archive failed'}
 & llvm-readobj --file-headers $library
 if($LASTEXITCODE){throw 'Owner/common archive inspection failed'}
 [ordered]@{source_commit=$commit;source_dirty=$false;target='Windows RT 8.1 ARM32';bt_dependency_source_commit=$bt;
  provider_contract_version=2;library='mt8167-hfp-owner-common.lib';sources=$sources;
  wdk_compile_archive_succeeded=$true;final_driver_link_verified=$false;
  required_external_module='Nash MT6630 HFP PnP/endpoint core + existing Audio backend';
  independent_dma_drain_implemented=$false;controller_registration_implemented=$false;
  pnp_endpoints_registered=$false;hci_bypass_capability_advertised=$false;hardware_verified=$false;
  existing_trial_media_modified=$false;sha256=(Get-FileHash $library -Algorithm SHA256).Hash.ToLowerInvariant()
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $out 'owner-common-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
