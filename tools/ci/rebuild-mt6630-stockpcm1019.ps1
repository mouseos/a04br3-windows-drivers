# MT6630 stock OTA FC72 packet rule; no sideband qualification or deployment.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory)
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '0C0942DE3294C99263A97B55EE12EC76C021EAA80963A4D90E08C471BC79E9BB'){throw 'Prior 2011 cache pin mismatch'}
$cm=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -ne '677c756958d29d3d09274d8256c8024023d9a1c7' -or $cm.total_objects -ne 76){throw 'Wrong paired generation'}
foreach($f in $cm.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers";$inc="$sdk\wdk\Include";$km="$tc\wdk\Lib\winv6.3\km\arm"
foreach($p in @(@{root=$tc;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Toolchain manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\provider","$out\audio","$out\hfp","$out\tmp"|Out-Null
# Only the toolchain root changes between runner VMs; no shared Git/config/SDK change.
$common=@($old.compiler_flags|ForEach-Object{$_.Replace('\\','\').Replace('C:\mt6630-migrated-cache-42404624946\restore',$RestoreDirectory)})
$ndis=@("-I$sdk\ndis-overlay")+$common+@('-std=c11','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$compiled=@();$reused=@();$provider=@();$saveTemp=$env:TEMP;$saveTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($r in @($old.reused_objects)+@($old.compiled_units)){
  $sourceHash=(Get-FileHash "$root\$($r.source)" -Algorithm SHA256).Hash
  if($r.object -in @('bt-transport.obj','ndis/checkpoint.obj')){
   $flags=if($r.object -eq 'ndis/checkpoint.obj'){$ndis}else{$common+@('-std=c11')}
   & "$tc\bin\clang.exe" @flags -c "$root\$($r.source)" -o "$out\$($r.object)";if($LASTEXITCODE){throw "Incremental compile failed $($r.source)"}
   $compiled+=@{source=$r.source;source_sha256=$sourceHash;object=$r.object;sha256=(Get-FileHash "$out\$($r.object)" -Algorithm SHA256).Hash}
  }else{
   if($sourceHash -ne $r.source_sha256){throw "Unexpected source delta $($r.source)"}
   Copy-Item "$prior\$($r.object)" "$out\$($r.object)";$reused+=$r
  }
  if($r.object.StartsWith('core/') -or $r.object.StartsWith('ndis/') -or $r.object -eq 'bt-transport.obj'){$provider+="$out\$($r.object)"}
 }
 if($provider.Count -ne 43 -or $compiled.Count -ne 2 -or $reused.Count -ne 74){throw 'Expected 43-provider/76-cache closure changed'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $provider "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys";if($LASTEXITCODE){throw 'Provider link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 @{source_commit=$commit;source_dirty=$false;previous_source=$cm.source_commit;compiled_units=$compiled;reused_objects=$reused;provider_objects=43;total_objects=76;compiled_this_update=2;reused_this_update=74;runtime_driver_build=1019;frozen_client_build=2011;frozen_audio_build=3003;frozen_pair_source=$cm.source_commit;stock_fc72_byte0_rule=99;stock_record_offset=8;remaining_pcm_bytes='dynamic record offsets9..11';input_record_modified=$false;pcm_bit_semantics_proven=$false;fc72_clock_format_qualified=$false;independent_dma_drain_available=$false;latency_available=$false;capabilities_current='None/0';hardware_verified=$false;device_access=$false;parent_sys_built=$false;media_included=$false;compiler_flags=$common;audio_flags=$old.audio_flags}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_STOCKPCM1019_INCREMENTAL_LINKED'
}finally{$env:TEMP=$saveTemp;$env:TMP=$saveTmp}
