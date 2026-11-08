# Standalone1012 QoS update; reuse only byte-identical core source/header closures.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory);$plan=Get-Content "$PSScriptRoot\mt6630-qos1014-core-cache.json" -Raw|ConvertFrom-Json
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne $plan.cache_manifest_sha256){throw 'Cache manifest mismatch'}
$cm=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -ne $plan.cache_source){throw 'Cache generation mismatch'}
foreach($f in $cm.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers";$km="$tc\wdk\Lib\winv6.3\km\arm"
foreach($p in @(@{root=$tc;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Toolchain manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\tmp"|Out-Null
$common=@($old.compiler_flags|ForEach-Object{$_.Replace('\\','\')})
$diag=@('-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1')
$ndis=@("-I$sdk\ndis-overlay")+$common+$diag+@('-std=c11','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT')
$compiled=@();$reused=@();$objects=@();$saveTemp=$env:TEMP;$saveTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($r in $plan.units){
  foreach($d in $r.dependencies){if((Get-FileHash "$root\$($d.path)" -Algorithm SHA256).Hash -ne $d.sha256){throw 'Source/header closure mismatch'}}
  if($r.reuse){
   if((Get-FileHash "$prior\$($r.object)" -Algorithm SHA256).Hash -ne $r.sha256){throw 'Object mismatch'}
   Copy-Item "$prior\$($r.object)" "$out\$($r.object)";$reused+=$r
  }else{
   & "$tc\bin\clang.exe" @common @diag -std=c11 -c "$root\$($r.source)" -o "$out\$($r.object)";if($LASTEXITCODE){throw "Core compile failed $($r.source)"}
   $compiled+=@{source=$r.source;object=$r.object;sha256=(Get-FileHash "$out\$($r.object)").Hash}
  }
  $objects+="$out\$($r.object)"
 }
 foreach($name in @('checkpoint','ndis_runtime','native_control','native_contract','native_security','station_runtime','native_tx','native_scan','native_scan_bridge','native_nbl','native_receive','native_hif_rx')){
  $source="drivers/network/mt6630/$name.c";$object="ndis/$name.obj"
  & "$tc\bin\clang.exe" @ndis -c "$root\$source" -o "$out\$object";if($LASTEXITCODE){throw "NDIS compile failed $name"}
  $compiled+=@{source=$source;object=$object;sha256=(Get-FileHash "$out\$object").Hash};$objects+="$out\$object"
 }
 if($objects.Count -ne 41){throw 'Standalone41-object closure required'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt6630-runtime.map" "/out:$out\mt6630-runtime.sys";if($LASTEXITCODE){throw 'Standalone link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" $out
 @{source_commit=$commit;source_dirty=$false;baseline_source='b21b869348e0608aa0ad08530ba74313130d5196';cache_source=$cm.source_commit;compiled_units=$compiled;reused_objects=$reused;compiled_this_update=$compiled.Count;reused_this_update=$reused.Count;total_objects=$objects.Count;runtime_driver_build=1014;standalone_wifi=$true;bt_broker_linked=$false;hardware_verified=$false;device_access=$false;parent_sys_built=$false;media_included=$false;compiler_flags=$common;ndis_flags=$ndis}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_QOS1014_INCREMENTAL_LINKED'
}finally{$env:TEMP=$saveTemp;$env:TMP=$saveTmp}
