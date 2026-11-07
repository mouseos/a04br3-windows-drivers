# Actual PortCls DriverEntry/AddDevice/StartDevice final SYS. No parent/media.
param([Parameter(Mandatory=$true)][string]$PreviousCacheDirectory,
 [Parameter(Mandatory=$true)][string]$RestoreDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root -c core.autocrlf=false status --porcelain).Count){throw 'Clean fixed source required'}
$prior=[IO.Path]::GetFullPath($PreviousCacheDirectory);if((Get-FileHash "$prior\cache-manifest.json" -Algorithm SHA256).Hash -ne '9F005C5874D48E034FDAD1C80D5CE7DA9151AE7D4E54C9E97BD2D4E08C73F541'){throw 'Prior cache manifest pin mismatch'};$cm=Get-Content "$prior\cache-manifest.json" -Raw|ConvertFrom-Json
if($cm.source_commit -ne 'e4b66080c9c9f486e0c36eb54228deabe7f88815' -or $cm.total_objects -ne 53){throw 'Pinned53 object generation required'}
foreach($f in $cm.files){if((Get-FileHash "$prior\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Prior cache bytes changed'}}
$old=Get-Content "$prior\build-manifest.json" -Raw|ConvertFrom-Json
$tc="$RestoreDirectory\compiler-libs";$sdk="$RestoreDirectory\sdk-headers";$crt="$RestoreDirectory\crt-headers";$km="$tc\wdk\Lib\winv6.3\km\arm"
foreach($entry in @(@{root=$tc;pin='3A0870CB8D233686223206E52E9F88F5A6553FD09D77DA1A9160B96F32859EA2'},@{root=$sdk;pin='78ED33B4C112A16053572DB34E8DB4F3CF077F9CA0AB38ED8D49117B4911BC3F'},@{root=$crt;pin='B75AE4E718976E5DB3CE535D9DE730249D19490AB49C48F46467FE98547A379C'})){
 if((Get-FileHash "$($entry.root)\files-manifest.json" -Algorithm SHA256).Hash -ne $entry.pin){throw 'Fixed SDK/compiler manifest changed'}
 $files=Get-Content "$($entry.root)\files-manifest.json" -Raw|ConvertFrom-Json
 foreach($f in $files.files){if((Get-FileHash "$($entry.root)\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Fixed SDK/compiler bytes changed'}}
}
$common=@($old.compiler_flags)+@("-I$root\drivers\bluetooth\mt6630")
$flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1','-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1','-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1','-DA04_AUDIO_DIGITAL_PLAYBACK=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_HFP_PROVIDER=1')
New-Item -ItemType Directory $out,"$out\audio","$out\reused","$out\tmp"|Out-Null
$objects=@();$reused=@();$compiled=@()
foreach($f in $old.compiled_units|Where-Object{$_.object.StartsWith('hfp/')}){
 if((Get-FileHash "$root\$($f.source)" -Algorithm SHA256).Hash -ne $f.source_sha256){throw 'Reused paired source changed'}
 $headers=@(& git -C $root diff --name-only e4b66080 HEAD -- drivers/bluetooth/mt6630 drivers/audio/mt8167-afe/hfp_provider.h drivers/audio/mt8167-afe/hfp_memif.h drivers/bus/mt8167-shared/hfp_interface.h)
 if($headers.Count){throw 'Paired headers changed; cannot reuse objects'}
 $name=[IO.Path]::GetFileName($f.object);Copy-Item "$prior\$($f.object)" "$out\reused\$name";$objects+="$out\reused\$name";$reused+=$f
}
if($reused.Count -ne 8){throw 'Eight unchanged paired objects required'}
function Compile([string]$relative,[string]$name,[bool]$cpp){
 $audioCompileFlags=@($common)+$flags
 if($cpp){$audioCompileFlags+=@('-std=c++14','-fno-exceptions','-fno-rtti')}else{$audioCompileFlags+=@('-std=c11')}
 & "$tc\bin\clang.exe" @audioCompileFlags -c "$root\$relative" -o "$out\audio\$name.obj"
 if($LASTEXITCODE){throw "Final Audio compile failed $relative"}
 $script:objects+="$out\audio\$name.obj";$script:compiled+=@{source=$relative;source_sha256=(Get-FileHash "$root\$relative" -Algorithm SHA256).Hash;object="audio/$name.obj";sha256=(Get-FileHash "$out\audio\$name.obj" -Algorithm SHA256).Hash}
}
$savedTemp=$env:TEMP;$savedTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($name in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query')){Compile "drivers/audio/mt8167-afe/$name.c" $name $false}
 Compile 'drivers/bus/mt8167-shared/clock_owner.c' 'clock_owner' $false
 foreach($name in @('adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter','stream_transport','pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','hfp_power')){Compile "drivers/audio/mt8167-afe/$name.cpp" $name $true}
 # noref retains the real HFP/provider path as well as DriverEntry; no
 # library-only success or dead-stripped placeholder is accepted here.
 & "$tc\bin\lld-link.exe" /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:noref $objects "$km\portcls.lib" "$km\ks.lib" "$km\ksguid.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\mt8167-audio.sys"
 if($LASTEXITCODE){throw 'Final Audio DriverEntry/HFP link failed'}
 Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-sco-candidate.inf" $out
 @{source_commit=$commit;source_dirty=$false;reused_pair_source=$old.source_commit;reused_objects=$reused;compiled_units=$compiled;reused_count=8;compiled_count=19;entrypoint='DriverEntry';portcls_slots=6;hfp_slots=4;opt_ref=$false;final_audio_sys_link_verified=$true;parent_built=$false;audio_flags=$flags;hardware_verified=$false;controller_registration_complete=$false;fc72_clock_format_qualified=$false;independent_dma_drain_available=$false;latency_available=$false;capabilities_current='None/0';sys_sha256=(Get-FileHash "$out\mt8167-audio.sys" -Algorithm SHA256).Hash}|ConvertTo-Json -Depth 9|Set-Content -Encoding UTF8 "$out\final-audio-manifest.json"
 'MT8167_MT6630_FINAL_AUDIO_DRIVERENTRY_LINKED'
}finally{$env:TEMP=$savedTemp;$env:TMP=$savedTmp}
