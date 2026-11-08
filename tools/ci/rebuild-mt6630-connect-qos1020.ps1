# Merge1013 connection gates with1014 QoS; three changed objects, no deployment.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$before='6e6bd36b4b05d345b1b468a7cf988ea93e55e1bd';$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory)
$changed=@(& git -C $root diff --name-only $before HEAD -- drivers/network/mt6630)
$allowed=@('drivers/network/mt6630/checkpoint.c','drivers/network/mt6630/mt6630-runtime.inf','drivers/network/mt6630/native_control.c','drivers/network/mt6630/station_runtime.c','drivers/network/mt6630/rsn_policy.h','drivers/network/mt6630/CONNECT-DATA-1013.md','drivers/network/mt6630/CONNECT-QOS-1020.md','drivers/network/mt6630/WPA2.md')
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unexpected source/header delta: cache cannot be reused'}
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '58dc38f5fab64c2622031755ec62f600cf6017502b4d1dd819c8969a2ed2b45b'){throw 'Cache manifest mismatch'}
$cm=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -ne $before -or $cm.total_objects -ne 41){throw 'Wrong cache generation'}
foreach($f in $cm.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$km="$tc\wdk\Lib\winv6.3\km\arm"
# Reuse exactly the1014 compiler/SDK/CRT and NDIS flags (no shared SDK setup).
foreach($dir in @('compiler-libs','sdk-headers','crt-headers')){
 $files=Get-Content "$RestoreDirectory\$dir\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$RestoreDirectory\$dir\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\tmp"|Out-Null
$ndis=@($old.ndis_flags)
$compiled=@();$reused=@();$objects=@();$saveTemp=$env:TEMP;$saveTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($r in @($old.reused_objects)+@($old.compiled_units)){
  $dest="$out\$($r.object)"
  if($r.object -in @('ndis/checkpoint.obj','ndis/native_control.obj','ndis/station_runtime.obj')){
   & "$tc\bin\clang.exe" @ndis -c "$root\$($r.source)" -o $dest;if($LASTEXITCODE){throw "Incremental compile failed $($r.source)"}
   $compiled+=@{source=$r.source;source_sha256=(Get-FileHash "$root\$($r.source)").Hash;object=$r.object;sha256=(Get-FileHash $dest).Hash}
  }else{Copy-Item "$prior\$($r.object)" $dest;$reused+=$r}
  $objects+=$dest
 }
 if($objects.Count -ne 41 -or $compiled.Count -ne 3 -or $reused.Count -ne 38){throw 'Expected3-compile/38-reuse standalone closure'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\mt6630-runtime.map" "/out:$out\mt6630-runtime.sys";if($LASTEXITCODE){throw 'Standalone link failed'}
 Copy-Item "$root\drivers\network\mt6630\mt6630-runtime.inf" $out
 @{source_commit=$commit;source_dirty=$false;baseline_source='b21b869348e0608aa0ad08530ba74313130d5196';qos_source=$before;connection_source='8fd9b62d5af47691a40b2726311b5415ff46aa4a';cache_source=$cm.source_commit;compiled_units=$compiled;reused_objects=$reused;compiled_this_update=3;reused_this_update=38;total_objects=41;runtime_driver_build=1020;standalone_wifi=$true;bt_broker_linked=$false;hardware_verified=$false;device_access=$false;parent_sys_built=$false;media_included=$false;compiler_flags=$old.compiler_flags;ndis_flags=$old.ndis_flags}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_CONNECT_QOS1020_INCREMENTAL_LINKED'
}finally{$env:TEMP=$saveTemp;$env:TMP=$saveTmp}
