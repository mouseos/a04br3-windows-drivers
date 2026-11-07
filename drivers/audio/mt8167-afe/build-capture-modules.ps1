# Source candidate only. No parent/SYS/INF/CAT/signing/deployment or device I/O.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New output inside this checkout required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $flags=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
 '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions',
 '-D__ldrexd=__capture_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',
 '-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_CAPTURE=1',"-I$inc\km","-I$inc\shared","-I$inc\um")
 $units=@('ul_engine.c','capture_owner.cpp','wave_dma_live.cpp','wavert_dma.cpp','dma_adapter.cpp','wavert_miniport.cpp','topology_miniport.cpp','wavert_registration.cpp','../../bus/mt8167-shared/codec_capture.c','../../bus/mt8167-shared/capture_input_owner.c')
 $objects=@();$sources=@()
 foreach($name in $units){
  $source=Join-Path $PSScriptRoot $name;$obj=Join-Path $out ([IO.Path]::GetFileName($name)+'.obj');$objects+=$obj
  if($name.EndsWith('.cpp')){& clang++ @flags -std=c++14 -fno-exceptions -fno-rtti -c $source -o $obj}
  else{& clang @flags -std=c11 -c $source -o $obj}
  if($LASTEXITCODE){throw "Capture module compile failed: $name"}
  $sources+=@{path=$(if($name.StartsWith('../../bus/')){'drivers/'+$name.Substring(6)}else{'drivers/audio/mt8167-afe/'+$name});sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 $lib=Join-Path $out 'mt8167-capture-candidate.lib'
 & lld-link /lib /machine:arm @objects "/out:$lib";if($LASTEXITCODE){throw 'Capture archive failed'}
 & llvm-readobj --file-headers $lib;if($LASTEXITCODE){throw 'Archive inspection failed'}
 [ordered]@{source_commit=$commit;source_dirty=$false;target='Windows RT8.1 ARM32';sources=$sources;flags=$flags;object_count=$objects.Count;
  objects=@($objects|ForEach-Object{@{path=[IO.Path]::GetFileName($_);sha256=(Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant()}});
  library_sha256=(Get-FileHash $lib -Algorithm SHA256).Hash.ToLowerInvariant();compile_archive_succeeded=$true;final_driver_link_verified=$false;
  parent_recompiled=$false;parent_version_reserved=$false;pnp_capture_endpoint_registered=$false;physical_mic_verified=$false;hardware_verified=$false;
  missing_integration=@('real parent capture clock/rail/codec/pin ownership and rollback','actual FIFO/codec latency','version reservation before any SYS/INF/CAT build')
 }|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 (Join-Path $out 'capture-module-manifest.json')
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
