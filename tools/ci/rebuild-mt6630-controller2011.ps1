# Private provider1018/client2011/Audio3003. Optional HFP parent compile only.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory)
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '302119DE205323C38230C8E0D16C7D82A85B4CCDFB85DD7AE726583C30D93054'){throw 'Previous76 manifest pin mismatch'}
$cache=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
if($cache.source_commit -ne 'ddf5a585f0006d37bef408c11fc80872579fd74c' -or $cache.total_objects -ne 76){throw 'Wrong cache generation'}
foreach($f in $cache.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers";$inc="$sdk\wdk\Include";$km="$tc\wdk\Lib\winv6.3\km\arm";$wdf="$tc\wdk\Lib\wdf\kmdf\arm\1.13"
foreach($p in @(@{root=$tc;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Toolchain manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\provider","$out\bluetooth","$out\audio","$out\hfp","$out\tmp"|Out-Null
$common=@($old.compiler_flags);$records=@();$reused=@();$provider=@();$final=@();$clientObjects=@()
$ndis=@("-I$sdk\ndis-overlay")+$common+@('-std=c11','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$client=$common+@('-std=c11','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',"-I$inc\wdf\kmdf\1.13")
$parent=$client+@('-DA04_SHARED_HFP=1','-DA04_SHARED_BUTTON_D0_EXPERIMENT=1','-DA04_SHARED_CHILD_PROBE=1','-DA04_MT6392_WRITE_DIAGNOSTIC=1','-DA04_SHARED_QUERY_EXPERIMENTAL=1','-DA04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC=1','-DA04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC=1','-DA04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC=1','-DA04_SHARED_MT6392_DIAGNOSTIC=1','-DA04_SHARED_APMIXED_RESOURCE=1')
$redo=@('ndis/checkpoint.obj','ndis/combo_broker.obj','combo_client.obj','radio_binding.obj','combo_controller.obj','hfp/hfp_endpoint.c.obj','hfp/ks_endpoints.cpp.obj','hfp/hfp_topology.cpp.obj','hfp/hfp_pnp.cpp.obj','hfp/combo_hfp_binding.cpp.obj','hfp/hfp_provider.cpp.obj','audio/pnp_backend.obj','audio/hfp_power.obj','audio/controller_watch.obj','parent-hfp-compile-only.obj')
$saveTemp=$env:TEMP;$saveTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($r in @($old.reused_objects)+@($old.compiled_units)){
  $sourceHash=(Get-FileHash "$root\$($r.source)" -Algorithm SHA256).Hash
  if($r.object -notin $redo){
   if($sourceHash -ne $r.source_sha256){throw "Unplanned source change $($r.source)"}
   Copy-Item "$prior\$($r.object)" "$out\$($r.object)";$reused+=$r
  }else{
   if($r.object.StartsWith('ndis/')){$flags=$ndis}
   elseif($r.object.StartsWith('hfp/')){$flags=$common+@("-I$root\drivers\bluetooth\mt6630")}
   elseif($r.object.StartsWith('audio/')){$flags=$common+@($old.audio_flags)}
   elseif($r.object -eq 'parent-hfp-compile-only.obj'){$flags=$parent}
   else{$flags=$client}
   if($r.source.EndsWith('.cpp')){$flags+=@('-std=c++14','-fno-exceptions','-fno-rtti')}else{$flags+=@('-std=c11')}
   & "$tc\bin\clang.exe" @flags -c "$root\$($r.source)" -o "$out\$($r.object)";if($LASTEXITCODE){throw "Compile failed $($r.source)"}
   $records+=@{source=$r.source;source_sha256=$sourceHash;object=$r.object;sha256=(Get-FileHash "$out\$($r.object)" -Algorithm SHA256).Hash}
  }
  if($r.object.StartsWith('core/') -or $r.object.StartsWith('ndis/') -or $r.object -eq 'bt-transport.obj'){$provider+="$out\$($r.object)"}
  elseif($r.object.StartsWith('hfp/') -or $r.object.StartsWith('audio/')){$final+="$out\$($r.object)"}
  elseif($r.object -ne 'parent-hfp-compile-only.obj'){$clientObjects+="$out\$($r.object)"}
 }
 if($provider.Count -ne 43 -or $final.Count -ne 28 -or $clientObjects.Count -ne 4 -or $records.Count -ne 15 -or $reused.Count -ne 61){throw 'Expected object graph changed'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $provider "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys";if($LASTEXITCODE){throw 'Provider link failed'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref $clientObjects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\bluetooth\mt6630-combo-bluetooth.sys";if($LASTEXITCODE){throw 'Client link failed'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:noref $final "$km\portcls.lib" "$km\ks.lib" "$km\ksguid.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\audio\mt8167-audio.sys";if($LASTEXITCODE){throw 'Audio final link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\bluetooth"
 Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-sco-candidate.inf" "$out\audio"
 @{source_commit=$commit;source_dirty=$false;previous_source=$cache.source_commit;reused_objects=$reused;compiled_units=$records;compiled_this_update=15;reused_this_update=61;provider_objects=43;client_objects=4;final_audio_objects=28;parent_compile_only=$true;parent_sys_built=$false;controller_registration_source_connected=$true;provider_abi=3;parent_hfp_abi=3;latency_forwarding_implemented=$true;dma_latency_provider_present=$false;hardware_verified=$false;device_access=$false;fc72_clock_format_qualified=$false;independent_dma_drain_available=$false;latency_available=$false;capabilities_current='None/0';compiler_flags=$common;audio_flags=$old.audio_flags}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_CONTROLLER2011_FINAL_SYS_LINKED'
}finally{$env:TEMP=$saveTemp;$env:TMP=$saveTmp}
