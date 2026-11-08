# Audio1026 + read-only ACCDET1102 consumer. No parent/RTC/thermal/USB build.
param([Parameter(Mandatory=$true)][string]$KitDirectory,
 [Parameter(Mandatory=$true)][string]$PortclsLibrary,
 [Parameter(Mandatory=$true)][string]$ArmUserLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,
 [Parameter(Mandatory=$true)][string]$Inf2CatPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$ReuseAudioDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean source required'}
$baseline='2d6231727e8ed72b6d7dca4148debc101ac5aeef'
$allowed=@('drivers/audio/mt8167-afe/jack_property.h','drivers/audio/mt8167-afe/jack_sense.h','drivers/audio/mt8167-afe/topology_miniport.cpp','drivers/audio/mt8167-afe/topology_miniport.h','drivers/audio/mt8167-afe/wavert_registration.cpp','drivers/audio/mt8167-afe/mt8167-afe-jack1027.inf','drivers/audio/mt8167-afe/JACK-SENSE.md')
$changed=@(& git -C $root diff --name-only $baseline HEAD -- drivers/audio)
if($LASTEXITCODE -or @($changed|Where-Object{$_ -notin $allowed}).Count){throw 'Unrelated audio1026 changes forbidden'}
New-Item -ItemType Directory "$out\obj","$out\package\audio","$out\package\client","$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$inc=Join-Path $KitDirectory 'Include';$lib=Join-Path $KitDirectory 'Lib\winv6.3\km\arm'
$audio=Join-Path $root 'drivers\audio\mt8167-afe'
$common=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
 '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions','-w',
 '-D__ldrexd=__a04_capture_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM',
 '-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
$flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1',
 '-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1',
 '-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_DIGITAL_PLAYBACK=1',
 '-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1',
 '-DA04_AUDIO_START_PERSIST=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_CAPTURE=1')
# First build: only the small audio closure; later header fixes rebuild topology
# only, using the same dedicated source-pinned object directory.
$reuse=$null
if($ReuseAudioDirectory){
 $reuse=[IO.Path]::GetFullPath($ReuseAudioDirectory)
 if(!$reuse.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Checkout-local audio cache required'}
 $previous=Get-Content "$reuse\package\signed-manifest.json" -Raw|ConvertFrom-Json
 if($previous.source_commit -ne 'b9cde04a7c48df3b478d9a7e6cbc0842c3d0c3e5' -or !$previous.signed){throw 'Fixed initial jack1027 cache required'}
 $allowedReuse=@('drivers/audio/mt8167-afe/jack_sense.h')
 $drift=@(& git -C $root diff --name-only $previous.source_commit HEAD -- drivers/audio drivers/bus/mt8167-shared/clock_owner.c)
 if($LASTEXITCODE -or @($drift|Where-Object{$_ -notin $allowedReuse}).Count){throw 'Unrelated audio cache dependency drift'}
 foreach($f in $previous.files){if((Get-FileHash (Join-Path "$reuse\package" $f.path) -Algorithm SHA256).Hash -ne $f.sha256){throw 'Initial signed cache package changed'}}
}
$script:reusedObjects=@();$script:compiledObjects=@()
function Build-JackObject($name,$source,$obj,$arguments){
 if($reuse -and $name -ne 'topology_miniport'){
  $prior="$reuse\obj\$name.obj";Copy-Item $prior $obj
  $script:reusedObjects+=@{name=$name;sha256=(Get-FileHash $obj -Algorithm SHA256).Hash;source_commit=$previous.source_commit}
 }else{
  & clang @arguments -c $source -o $obj
  if($LASTEXITCODE){throw "Audio compile failed $name"}
  $script:compiledObjects+=$name
 }
}
$objects=@();$sources=@()
foreach($name in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query','capture_parent_query','ul_engine')){
 $source=Join-Path $audio ($name+'.c');$obj="$out\obj\$name.obj"
 Build-JackObject $name $source $obj ($common+$flags)
 $objects+=$obj;$sources+=$source
}
$source=Join-Path $root 'drivers\bus\mt8167-shared\clock_owner.c';$obj="$out\obj\clock_owner.obj"
Build-JackObject 'clock_owner' $source $obj $common
$objects+=$obj;$sources+=$source
foreach($name in @('adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter','capture_latency_gate','stream_transport','pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','start_record','capture_owner')){
 $source=Join-Path $audio ($name+'.cpp');$obj="$out\obj\$name.obj"
 Build-JackObject $name $source $obj ($common+$flags+@('-DA04_AUDIO_ACCDET_JACK=1','-std=c++14','-fms-compatibility','-fno-exceptions','-fno-rtti'))
 $objects+=$obj;$sources+=$source
}
$sys="$out\package\audio\mt8167-audio.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects $PortclsLibrary "$lib\ntoskrnl.lib" "$lib\hal.lib" "$lib\bufferoverflowfastfailk.lib" "$lib\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Audio link failed'}
Copy-Item "$audio\mt8167-afe-jack1027.inf" "$out\package\audio"
$client="$out\package\client\mt8167-afe-jack-read.exe"
if($reuse){
 Copy-Item "$reuse\package\client\mt8167-afe-jack-read.exe" $client
}else{
& clang --target=thumbv7-pc-windows-msvc -O2 -w -fno-builtin -fno-vectorize -fno-slp-vectorize -mfpu=none -mno-unaligned-access -fms-extensions -ffreestanding -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\audio\mt8167-afe-jack-read.c" -o "$out\obj\jack-client.obj"
if($LASTEXITCODE){throw 'Jack client compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\obj\jack-client.obj" "$ArmUserLibraryDirectory\kernel32.lib" "$ArmUserLibraryDirectory\setupapi.lib" "$ArmUserLibraryDirectory\cfgmgr32.lib" "/out:$client"
if($LASTEXITCODE){throw 'Jack client link failed'}
}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer key unavailable'}
foreach($f in @($sys,$client)){
 if(!$reuse -or $f -eq $sys){& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $f;if($LASTEXITCODE){throw 'Signing failed'}}
 & $SigntoolPath verify /pa /v $f;if($LASTEXITCODE){throw 'Signature verify failed'}
 if((Get-AuthenticodeSignature $f).SignerCertificate.Thumbprint -ne $thumb){throw 'Wrong signer'}
 $pe=& llvm-readobj --file-headers --coff-imports $f
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARM32 required'}
 $pe|Set-Content -Encoding UTF8 ($f+'.pe.txt')
}
& $Inf2CatPath "/driver:$out\package\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Audio catalog generation failed'}
$cat="$out\package\audio\mt8167-afe-jack1027.cat"
& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'Catalog sign failed'}
& $SigntoolPath verify /pa /v $cat
if($LASTEXITCODE -or (Get-AuthenticodeSignature $cat).SignerCertificate.Thumbprint -ne $thumb){throw 'Catalog signature failed'}
foreach($f in @($sys,"$out\package\audio\mt8167-afe-jack1027.inf")){
 & $SigntoolPath verify /pa /v /c $cat $f;if($LASTEXITCODE){throw 'CAT membership failed'}
}
Export-Certificate -Cert $cert -FilePath "$out\package\development.cer"|Out-Null
$files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
@{source_commit=$commit;baseline_audio_source=$baseline;baseline_audio_flags=$flags;flag_delta=@{A04_AUDIO_ACCDET_JACK='1'};driver_version='2026.11.8.1027';required_parent_accdet_source='4e253a5c5f53ccdc34c8aaa92296986d53f72d6a';client_reused=[bool]$reuse;required_parent_accdet_schema=1;required_parent_accdet_bytes=16;parent_built=$false;hardware_verified=$false;headphone_signal_route_added=$false;jack_event_advertised=$false;gpio_pmic_afe_writes_added=$false;signed=$true;signer_thumbprint=$thumb;compiler=((& clang --version)-join "`n");compiled_kernel_tus=$script:compiledObjects.Count;compiled_kernel_objects=$script:compiledObjects;reused_kernel_objects=$script:reusedObjects;files=$files}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive -Path "$out\package\*" -DestinationPath "$out\mt8167-afe-jack1027-signed.zip"
'MT8167_AFE_JACK1027_SIGNED_COMPLETE'
