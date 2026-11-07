# Private paired controller2010 + provider1017 + final Audio3002. No deploy.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$PreviousAudioCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ResumeDirectory, [string]$ResumeManifestSha256)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory);$audio=[IO.Path]::GetFullPath($PreviousAudioCacheDirectory)
function VerifyCache([string]$dir,[string]$source,[int]$count){
 $m=Get-Content "$dir\cache-manifest.json" -Raw|ConvertFrom-Json
 if($m.source_commit -ne $source -or $m.total_objects -ne $count){throw 'Wrong cache generation'}
 foreach($f in $m.files){if((Get-FileHash "$dir\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache bytes changed'}}
}
if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '9F005C5874D48E034FDAD1C80D5CE7DA9151AE7D4E54C9E97BD2D4E08C73F541'){throw 'Previous53 manifest pin mismatch'}
VerifyCache $prior 'e4b66080c9c9f486e0c36eb54228deabe7f88815' 53
if((Get-FileHash "$audio\cache-manifest.json" -Algorithm SHA256).Hash -ne 'F88A53A1D29BCFB406EC2A7BE4C49243E75C2A45B3E6223E14A7A347D62544D8'){throw 'Previous27 manifest pin mismatch'}
VerifyCache $audio 'faa0a86615ce6836c092a59240532c80c313d844' 27
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$am=Get-Content "$audio\final-audio-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers";$inc="$sdk\wdk\Include";$km="$tc\wdk\Lib\winv6.3\km\arm";$wdf="$tc\wdk\Lib\wdf\kmdf\arm\1.13"
foreach($p in @(@{root=$tc;hash='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;hash='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;hash='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($p.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $p.hash){throw 'Toolchain manifest mismatch'}
 $files=Get-Content "$($p.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($p.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Toolchain bytes changed'}}
}
New-Item -ItemType Directory $out,"$out\core","$out\ndis","$out\provider","$out\bluetooth","$out\audio","$out\hfp","$out\tmp"|Out-Null
$common=@($old.compiler_flags);$records=@();$reused=@();$provider=@();$final=@()
$ndis=@("-I$sdk\ndis-overlay")+$common+@('-std=c11','-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$client=$common+@('-std=c11','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',"-I$inc\wdf\kmdf\1.13")
function Compile([string]$source,[string]$object,[object[]]$flags){
 if($resumeFiles.ContainsKey($object) -and $object -ne 'parent-hfp-compile-only.obj') {Copy-Item "$ResumeDirectory\$object" "$out\$object"}
 else {& "$tc\bin\clang.exe" @flags -c "$root\$source" -o "$out\$object";if($LASTEXITCODE){throw "Compile failed $source"}}
 $script:records+=@{source=$source;source_sha256=(Get-FileHash "$root\$source" -Algorithm SHA256).Hash;object=$object;sha256=(Get-FileHash "$out\$object" -Algorithm SHA256).Hash}
}
function Reuse([string]$dir,$r,[string]$object){
 $sourceHash=(Get-FileHash "$root\$($r.source)" -Algorithm SHA256).Hash
 if($r.source_sha256){if($sourceHash -ne $r.source_sha256){throw "Reused source changed $($r.source)"}}
 else {& git -C $root diff --quiet e4b66080c9c9f486e0c36eb54228deabe7f88815 HEAD -- $r.source;if($LASTEXITCODE){throw "Unhashed legacy source changed $($r.source)"}}
 Copy-Item "$dir\$($r.object)" "$out\$object"
 $script:reused+=@{source=$r.source;source_sha256=$sourceHash;object=$object;sha256=(Get-FileHash "$out\$object" -Algorithm SHA256).Hash}
}
$resumeFiles=@{}
if($ResumeDirectory){
 if(!$ResumeManifestSha256 -or (Get-FileHash "$ResumeDirectory\..\..\..\resume-manifest.json" -Algorithm SHA256).Hash -ne $ResumeManifestSha256){throw 'Resume manifest pin mismatch'}
 $rm=Get-Content "$ResumeDirectory\..\..\..\resume-manifest.json" -Raw|ConvertFrom-Json
 if(!$rm.source_commit.StartsWith('7e351a90') -or $rm.source_dirty){throw 'Wrong resume source'}
 & git -C $root diff --quiet $rm.source_commit HEAD -- drivers;if($LASTEXITCODE){throw 'Resume driver sources changed'}
 foreach($f in $rm.files){if((Get-FileHash "$ResumeDirectory\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Resume object changed'};$resumeFiles[$f.path]=$f.sha256}
}
$saveTemp=$env:TEMP;$saveTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 # Nested combo layout changed: recompile all thirteen NDIS consumers.
 foreach($r in @($old.reused_objects)+@($old.compiled_units)){
  if($r.object.StartsWith('core/') -or $r.object -eq 'bt-transport.obj'){Reuse $prior $r $r.object;$provider+="$out\$($r.object)"}
  elseif($r.object.StartsWith('ndis/')){Compile $r.source $r.object $ndis;$provider+="$out\$($r.object)"}
 }
 if($provider.Count -ne 43){throw '43 provider objects required'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref $provider "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\provider\mt6630-combo-runtime.sys";if($LASTEXITCODE){throw 'Provider link failed'}
 $clientObjects=@();foreach($name in @('combo_client','radio_binding','combo_controller','hfp_target')){Compile "drivers/bluetooth/mt6630/$name.c" "$name.obj" $client;$clientObjects+="$out\$name.obj"}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref $clientObjects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\bluetooth\mt6630-combo-bluetooth.sys";if($LASTEXITCODE){throw 'Controller client link failed'}
 foreach($r in @($old.compiled_units)|Where-Object{$_.object.StartsWith('hfp/')}){
  if($r.source.EndsWith('/hfp_provider.cpp')){Compile $r.source $r.object ($common+@("-I$root\drivers\bluetooth\mt6630",'-std=c++14','-fno-exceptions','-fno-rtti'))}
  else{Reuse $prior $r $r.object}
  $final+="$out\$($r.object)"
 }
 if($final.Count -ne 8){throw 'Eight paired HFP objects required'}
 foreach($r in $am.compiled_units){
  if($r.source -in @('drivers/audio/mt8167-afe/pnp_backend.cpp','drivers/audio/mt8167-afe/hfp_power.cpp')){Compile $r.source $r.object ($common+@($am.audio_flags)+@('-std=c++14','-fno-exceptions','-fno-rtti'))}
  else{Reuse $audio $r $r.object}
  $final+="$out\$($r.object)"
 }
 Compile 'drivers/audio/mt8167-afe/controller_watch.cpp' 'audio/controller_watch.obj' ($common+@($am.audio_flags)+@('-std=c++14','-fno-exceptions','-fno-rtti'));$final+="$out\audio\controller_watch.obj"
 if($final.Count -ne 28){throw '28 final Audio objects required'}
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:noref $final "$km\portcls.lib" "$km\ks.lib" "$km\ksguid.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\audio\mt8167-audio.sys";if($LASTEXITCODE){throw 'Final Audio link failed'}
 # Optional parent v2 source compile only. No RTC profile/SYS/package change.
 $parent=$client+@('-DA04_SHARED_HFP=1','-DA04_SHARED_BUTTON_D0_EXPERIMENT=1','-DA04_SHARED_CHILD_PROBE=1','-DA04_MT6392_WRITE_DIAGNOSTIC=1','-DA04_SHARED_QUERY_EXPERIMENTAL=1','-DA04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC=1','-DA04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC=1','-DA04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC=1','-DA04_SHARED_MT6392_DIAGNOSTIC=1','-DA04_SHARED_APMIXED_RESOURCE=1')
 Compile 'drivers/bus/mt8167-shared/wdf_parent_probe.c' 'parent-hfp-compile-only.obj' $parent
 Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\provider"
 Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\bluetooth"
 Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-sco-candidate.inf" "$out\audio"
 @{source_commit=$commit;source_dirty=$false;reused_objects=$reused;compiled_units=$records;provider_objects=43;client_objects=4;final_audio_objects=28;parent_compile_only=$true;parent_sys_built=$false;controller_registration_source_connected=$true;resume_manifest_sha256=$ResumeManifestSha256;resumed_objects=$resumeFiles.Count;hardware_verified=$false;device_access=$false;fc72_clock_format_qualified=$false;independent_dma_drain_available=$false;latency_available=$false;capabilities_current='None/0';compiler_flags=$common;audio_flags=$am.audio_flags}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT6630_CONTROLLER2010_FINAL_SYS_LINKED'
}finally{$env:TEMP=$saveTemp;$env:TMP=$saveTmp}
