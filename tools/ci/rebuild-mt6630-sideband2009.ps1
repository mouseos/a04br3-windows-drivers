# Private combo1016/client2009 + real HFP/AFE paired modules. No media/deploy.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory)
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '2ED1C23BA8BFB90CD09087016B51B6A6F4E1BB9539076C2528DC63965A7DDD1A'){throw 'Combo1015 cache pin mismatch'}
$m=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
foreach($f in $m.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cached bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
if($old.source_commit -ne '607dc3a5141046b841eacd4fd69edcee8315a572'){throw 'Wrong previous source'}
$changed=@(& git -C $root diff --name-only 607dc3a5 HEAD -- drivers/network)
if(@($changed|Where-Object{$_ -notin @('drivers/network/mt6630/checkpoint.c','drivers/network/mt6630/combo_broker.c','drivers/network/mt6630/mt6630-combo-runtime.inf')}).Count){throw 'Unchanged NDIS/core layout required'}
if(@(& git -C $root diff --name-only 607dc3a5 HEAD -- drivers/bluetooth/mt6630/transport.c drivers/bluetooth/mt6630/transport.h drivers/network/mt6630/ndis_runtime.h drivers/network/mt6630/combo_broker.h).Count){throw 'Cached nested layout/transport changed'}
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers"
foreach($p in @(@{root=$tc;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Toolchain manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\provider","$out\bluetooth","$out\hfp","$out\tmp"|Out-Null
$inc="$sdk\wdk\Include";$km="$tc\wdk\Lib\winv6.3\km\arm";$wdf="$tc\wdk\Lib\wdf\kmdf\arm\1.13"
$common=@('--target=thumbv7-pc-windows-msvc','-resource-dir',"$sdk\clang-resource",'-nostdinc','-isystem',"$sdk\clang-resource\include",'-isystem',"$crt\msvc-include",'-isystem',"$crt\atlmfc-include",'-isystem',"$crt\ucrt",'-fms-compatibility-version=19.44.35229','-w','-ffunction-sections','-fdata-sections','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
$ndis=$common+@('-std=c11','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1',"-I$sdk\ndis-overlay")
$reused=@();$objects=@();$compiled=@();$hfpObjects=@()
foreach($r in @($old.reused_objects)+@($old.compiled_units)){
 if($r.object -in @('ndis/checkpoint.obj','ndis/combo_broker.obj')){continue}
 Copy-Item "$prior\$($r.object)" "$out\$($r.object)";$objects+="$out\$($r.object)";$reused+=$r
}
if($reused.Count -ne 41){throw 'Expected 41 unchanged provider objects'}
function Compile([string]$relative,[string]$object,[object[]]$flags){
 & "$tc\bin\clang.exe" @flags -c "$root\$relative" -o "$out\$object"
 if($LASTEXITCODE){throw "Incremental compile failed $relative"}
 $script:compiled+=@{source=$relative;source_sha256=(Get-FileHash "$root\$relative" -Algorithm SHA256).Hash;object=$object;sha256=(Get-FileHash "$out\$object" -Algorithm SHA256).Hash}
}
$savedTemp=$env:TEMP;$savedTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($name in @('checkpoint','combo_broker')){Compile "drivers/network/mt6630/$name.c" "ndis/$name.obj" $ndis;$objects+="$out\ndis\$name.obj"}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys"
 if($LASTEXITCODE){throw 'Provider link failed'}
 $client=$common+@('-std=c11','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',"-I$inc\wdf\kmdf\1.13")
 foreach($name in @('combo_client','radio_binding')){Compile "drivers/bluetooth/mt6630/$name.c" "$name.obj" $client}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref "$out\combo_client.obj" "$out\radio_binding.obj" "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\bluetooth\mt6630-combo-bluetooth.sys"
 if($LASTEXITCODE){throw 'Sideband client link failed'}
 $hfp=$common+@("-I$root\drivers\bluetooth\mt6630")
 foreach($name in @('sideband.c','hfp_endpoint.c','ks_endpoints.cpp','hfp_topology.cpp','hfp_pnp.cpp','combo_hfp_binding.cpp')){
  $flags=$hfp;if($name.EndsWith('.cpp')){$flags+=@('-std=c++14','-fno-exceptions','-fno-rtti')}else{$flags+=@('-std=c11')}
  Compile "drivers/bluetooth/mt6630/$name" "hfp/$name.obj" $flags;$hfpObjects+="$out\hfp\$name.obj"
 }
 foreach($name in @('hfp_memif.c','hfp_provider.cpp')){
  $flags=$hfp;if($name.EndsWith('.cpp')){$flags+=@('-std=c++14','-fno-exceptions','-fno-rtti')}else{$flags+=@('-std=c11')}
  Compile "drivers/audio/mt8167-afe/$name" "hfp/$name.obj" $flags;$hfpObjects+="$out\hfp\$name.obj"
 }
 & "$tc\bin\lld-link.exe" /lib /machine:arm $hfpObjects "/out:$out\mt8167-mt6630-sideband-paired.lib"
 if($LASTEXITCODE){throw 'Paired sideband library failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\bluetooth"
 @{source_commit=$commit;source_dirty=$false;previous_source=$old.source_commit;reused_objects=$reused;compiled_units=$compiled;provider_reused_objects=41;provider_compiled_units=2;client_compiled_units=2;paired_sideband_units=8;hfp_source='dd31bcc418a85e17017b4802d0e68227df473677';audio_hfp_source='6b5decfe17635e701d00b8315f7bfca0a7df4bca';runtime_driver_build=1016;client_build=2009;parent_built=$false;audio_sys_built=$false;paired_library_final_audio_link_verified=$false;controller_registration_complete=$false;fc72_decoded=$false;independent_dma_drain_available=$false;latency_available=$false;capability_default='None/0';hardware_verified=$false;standard_stack_compatible=$false;media_included=$false;compiler_flags=$common}|ConvertTo-Json -Depth 9|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_SIDEBAND2009_INCREMENTAL_LINKED'
}finally{$env:TEMP=$savedTemp;$env:TMP=$savedTmp}
