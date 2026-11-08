param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$Kit,
 [Parameter(Mandatory=$true)][string]$UmLib,[Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ReuseAudioDirectory,[string]$ClientKit="C:\mt8167-minimal-service-host\cache\kit81\Include")
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$Inputs\closure-manifest.json" -Raw|ConvertFrom-Json
foreach($x in $m.files){if((Sha (Join-Path $Inputs $x.path)) -cne $x.sha256){throw "Small closure drift $($x.path)"}}
$cache=Get-Content "$Inputs\parent-cache.json" -Raw|ConvertFrom-Json
if(((& clang --version)[0]) -cne (($cache.compiler -split "`n")[0])){throw 'Compiler drift'}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\audio","$out\package\clients"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$common=@($cache.common|Where-Object{$_ -ne '-std=c11'})
$flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1','-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1','-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_DIGITAL_PLAYBACK=1','-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1','-DA04_AUDIO_START_PERSIST=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_CAPTURE=1','-DA04_AUDIO_ACCDET_JACK=1','-DA04_AUDIO_HP_COMPONENT=1','-DA04_AUDIO_HP_SHARED=1')
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$Kit\km",'-isystem',"$Kit\shared",'-isystem',"$Kit\um")
$objects=@();$compiled=@()
if($ReuseAudioDirectory){
 if(@(& git -C $root diff --name-only 950f0a3fc61c677b6191921f35fb259a8e4841f4 HEAD -- drivers).Count){throw 'Resumed audio source changed'}
}
foreach($name in $m.compile_required){
 $obj="$out\obj\$name.obj"
 if($ReuseAudioDirectory){Copy-Item "$ReuseAudioDirectory\obj\$name.obj" $obj;$objects+=$obj;continue}
 & clang @common @kernel @flags -std=c++14 -fms-compatibility -fno-exceptions -fno-rtti -w -c "$root\drivers\audio\mt8167-afe\$name.cpp" -o $obj
 if($LASTEXITCODE){throw "Audio compile failed $name"};$objects+=$obj;$compiled+=$name
}
foreach($x in $m.reused_objects){$objects+="$Inputs\obj\$($x.name).obj"}
if($objects.Count -ne 24 -or $flags.Count -ne 17){throw 'Complete HP audio flags/object tuple required'}
$sys="$out\package\audio\mt8167-audio.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects "$Inputs\portcls.lib" "$Inputs\km\ntoskrnl.lib" "$Inputs\km\hal.lib" "$Inputs\km\bufferoverflowfastfailk.lib" "$Inputs\km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Audio link failed'}
Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-hp1033.inf" "$out\package\audio"
# Exact standard COM API imports used by the existing signed WASAPI client.
& lld-link /lib /machine:arm "/def:$root\tools\ci\mt8167-silent-ole32.def" "/out:$out\obj\ole32.lib"
if($LASTEXITCODE){throw 'COM import library failed'}
$client="$out\package\clients\mt8167-silent-pcm-consumer.exe"
$user=@('-D__ldrexd=__a04_silent_pcm_wdk81_ldrexd_declaration','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-nostdlibinc','-isystem',"$ClientKit\km\crt",'-isystem',"$ClientKit\um",'-isystem',"$ClientKit\shared")
& clang @common @user -std=c11 -w "-DA04_ENGINE_PROBE_SOURCE=0x$($commit.Substring(0,8))u" -c "$root\tools\audio\mt8167-silent-pcm-consumer.c" -o "$out\obj\silent-consumer.obj"
if($LASTEXITCODE){throw 'Silent consumer compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\obj\silent-consumer.obj" "$out\obj\ole32.lib" "$UmLib\kernel32.lib" "$UmLib\setupapi.lib" "$UmLib\cfgmgr32.lib" "$UmLib\advapi32.lib" "/out:$client"
if($LASTEXITCODE){throw 'Silent consumer link failed'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb";if(!$cert.HasPrivateKey){throw 'Existing signer required'}
$unsigned=@()
foreach($p in @($sys,$client)){
 $unsigned+=@{path=$p;sha256=(Sha $p)}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $p;if($LASTEXITCODE){throw 'PE signing failed'}
 & $SignTool verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'PE signature mismatch'}
 $pe=& llvm-readobj --file-headers --coff-imports $p;if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'};$pe|Set-Content -Encoding UTF8 ($p+'.pe.txt')
}
& "$Inputs\signing\bin\Inf2Cat.exe" "/driver:$out\package\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
$cat="$out\package\audio\mt8167-afe-hp1033.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
& $SignTool verify /pa /v $cat;if($LASTEXITCODE){throw 'CAT verify failed'}
foreach($p in @($sys,"$out\package\audio\mt8167-afe-hp1033.inf")){& $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
@{schema='mt8167-hp1033-silent';source_commit=$commit;audio_version='2026.11.8.1033';hp_abi_version=2;hp_abi_build=1107;thermal_qi_build=1104;audio_flags=$flags;compiled_audio=$compiled;recovered_audio_objects=$m.compile_required;audio_object_source='950f0a3fc61c677b6191921f35fb259a8e4841f4';compiled_clients=@('silent-consumer');reused=$m.reused_objects;previous1031_invalid_flags_corrected=$true;all_six_names_static=$true;parent_rebuilt=$false;sdk_rebuilt=$false;signed=$true;signer_thumbprint=$thumb;hardware_verified=$false;audible_verified=$false;standard_hw_latency_known=$false;fabric_full_drain_proved=$false;payload='one second all-zero PCM per fixed route invocation';unsigned=$unsigned;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\hp1033-signed.zip"
Compress-Archive "$out\obj\*" "$out\hp1033-objects.zip"
Get-FileHash "$out\hp1033-signed.zip","$out\hp1033-objects.zip" -Algorithm SHA256
'MT8167_HP1033_SILENT_SIGNED_NOT_DEVICE_TESTED'
