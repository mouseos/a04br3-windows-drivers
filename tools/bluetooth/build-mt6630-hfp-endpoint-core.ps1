# WDK8.1 ARM32: BT-private integration library only; no SYS/sign/deploy/runner.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-hfp-endpoint-core'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE){throw 'Cannot identify source commit'}
$dirty=@(& git -C $root status --porcelain);if($LASTEXITCODE -or $dirty.Count){throw 'Build requires a clean fixed source commit'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$manifest=Join-Path $out 'hfp-endpoint-manifest.json'
if(Test-Path $manifest){Move-Item $manifest (Join-Path $out ('manifest.previous.'+[Guid]::NewGuid().ToString('N')+'.json'))}
$priorTmp=$env:TMP;$priorTemp=$env:TEMP
try {
 $env:TMP=Join-Path $out 'tmp';$env:TEMP=$env:TMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 foreach($header in @('km\portcls.h','km\bthhfpddi.h','shared\ks.h','shared\ksmedia.h')){
  if(!(Test-Path (Join-Path $inc $header))){throw "Missing WDK8.1 dependency: $header"}
 }
 $kernelCodegenFlags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
 $common=@('--target=thumbv7-pc-windows-msvc','-O2','-fno-stack-protector','-ffreestanding','-fms-extensions',
  '-ffunction-sections','-fdata-sections','-D_KERNEL_MODE','-D_ARM_','-DARM',
  '-D__ldrexd=__bt_wdk81_ldrexd_declaration','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',"-I$inc\km","-I$inc\shared","-I$inc\um")+$kernelCodegenFlags
 $objects=@();$sources=@()
 foreach($name in @('sideband.c','hfp_endpoint.c','ks_endpoints.cpp','hfp_topology.cpp','hfp_pnp.cpp')){
  $source=Join-Path $root "drivers\bluetooth\mt6630\$name"
  $obj=Join-Path $out ($name+'.obj');$objects+=$obj
  if($name.EndsWith('.cpp')){
   & clang++ @common -std=c++14 -fno-exceptions -fno-rtti -c $source -o $obj
  } else {
   & clang @common -std=c11 -c $source -o $obj
  }
  if($LASTEXITCODE){throw "HFP endpoint $name compile failed"}
  $sources+=@{path="drivers/bluetooth/mt6630/$name";sha256=(Get-FileHash $source -Algorithm SHA256).Hash}
 }
 foreach($name in @('sideband.h','hfp_endpoint.h','ks_endpoints.h','hfp_topology.h','hfp_pnp.h','hfp_volume.h')){
  $path="drivers/bluetooth/mt6630/$name"
  $sources+=@{path=$path;sha256=(Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash}
 }
 $library=Join-Path $out 'mt6630-hfp-endpoint-core.lib'
 if(Test-Path $library){Remove-Item $library}
 & lld-link /lib /machine:arm @objects "/out:$library"
 if($LASTEXITCODE){throw 'HFP endpoint integration library archive failed'}
 Copy-Item "$root\drivers\bluetooth\mt6630\HFP-ENDPOINTS.md" $out -Force
 Copy-Item "$root\drivers\bluetooth\mt6630\hfp-interfaces.inf.inc" $out -Force
 Copy-Item "$root\drivers\bluetooth\mt6630\PROVIDER-EVIDENCE.md" $out -Force
 @{source_commit=$commit;source_dirty=$false;sources=$sources;target='Windows RT 8.1 ARM32';
   library='mt6630-hfp-endpoint-core.lib';provider_contract_version=2;
   kernel_codegen_flags=$kernelCodegenFlags;kernel_compile_flags=$common;
   final_link_dependencies=@('ntoskrnl.lib','hal.lib','portcls.lib','libcntpr.lib','bufferoverflowfastfailk.lib');
   hfp_oneshot_slc_requests=$true;hfp_request_acceptance_is_connection_success=$false;bt_private_normal_final_release_deferred=$true;no_effects_acquire_refusal_retryable=$true;mono_all_channel_volume=$true;wdk_compile_archive_succeeded=$true;
   final_driver_link_verified=$false;audio_v11_provider_connected=$false;pnp_endpoints_registered=$false;topology_source_implemented=$true;hfp_volume_sync_source_implemented=$true;hfp_jack_notifications_source_implemented=$true;pnp_lifecycle_source_implemented=$true;audio_fdo_callsite_connected=$false;
   standard_stack_contract_satisfied=$false;hardware_verified=$false;
   advertised_sco='None (unchanged)';exclusive_bluetooth=$true;wifi_coexistence_verified=$false;
   format='CVSD (Windows bypass codec 0), 8000 Hz mono S16_LE';
   sha256=(Get-FileHash $library -Algorithm SHA256).Hash
 }|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 $manifest
} finally {$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
