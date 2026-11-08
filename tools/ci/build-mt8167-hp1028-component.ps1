# Signed component build only: no fabricated HP provider or activation.
param([Parameter(Mandatory=$true)][string]$AudioCache,
 [Parameter(Mandatory=$true)][string]$KitDirectory,
 [Parameter(Mandatory=$true)][string]$PortclsLibrary,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,
 [Parameter(Mandatory=$true)][string]$Inf2CatPath,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean source required'}
$m=Get-Content "$AudioCache\package\signed-manifest.json" -Raw|ConvertFrom-Json
if($m.source_commit -ne '81f20c785cebf7e7a65778a3b3a10687bb6bcb54' -or !$m.signed){throw 'Fixed final AFE1027 cache required'}
foreach($f in $m.files){if((Get-FileHash (Join-Path "$AudioCache\package" $f.path) -Algorithm SHA256).Hash -ne $f.sha256){throw 'Cache signed package changed'}}
$allowed=@('drivers/audio/mt8167-afe/adapter.cpp','drivers/audio/mt8167-afe/headphone_endpoint.h','drivers/audio/mt8167-afe/headphone_endpoint.cpp','drivers/audio/mt8167-afe/mt8167-afe-hp1028.inf','drivers/audio/mt8167-afe/HEADPHONE.md','drivers/bus/mt8167-shared/codec_headphone.h','drivers/bus/mt8167-shared/codec_headphone.c')
$drift=@(& git -C $root diff --name-only $m.source_commit HEAD -- drivers)
if($LASTEXITCODE -or @($drift|Where-Object{$_ -notin $allowed}).Count){throw 'Unrelated owner/audio changes forbidden'}
New-Item -ItemType Directory "$out\obj","$out\package\audio","$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$inc="$KitDirectory\Include";$lib="$KitDirectory\Lib\winv6.3\km\arm"
$common=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions','-w','-D__ldrexd=__a04_capture_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
$flags=@($m.baseline_audio_flags)+@('-DA04_AUDIO_ACCDET_JACK=1','-DA04_AUDIO_HP_COMPONENT=1')
$objects=@();$reuse=@()
foreach($prior in Get-ChildItem "$AudioCache\obj" -Filter '*.obj'|Where-Object{$_.BaseName -notin @('adapter','jack-client')}){
 $obj="$out\obj\$($prior.Name)";Copy-Item $prior.FullName $obj;$objects+=$obj;$reuse+=@{name=$prior.Name;sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
}
if($reuse.Count -ne 22){throw 'Exactly22 fixed AFE objects required'}
foreach($name in @('adapter','headphone_endpoint')){
 $obj="$out\obj\$name.obj";$source="$root\drivers\audio\mt8167-afe\$name.cpp"
 & clang @common @flags -std=c++14 -fms-compatibility -fno-exceptions -fno-rtti -c $source -o $obj
 if($LASTEXITCODE){throw "HP component C++ compile failed $name"};$objects+=$obj
}
$obj="$out\obj\codec_headphone.obj"
& clang @common -c "$root\drivers\bus\mt8167-shared\codec_headphone.c" -o $obj
if($LASTEXITCODE){throw 'HP analog component compile failed'};$objects+=$obj
$sys="$out\package\audio\mt8167-audio.sys"
# Keep the usable component entrypoints in this build proof. They are NOT
# auto-called by the current adapter: no real measured HP provider yet exists.
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects $PortclsLibrary "$lib\ntoskrnl.lib" "$lib\hal.lib" "$lib\bufferoverflowfastfailk.lib" "$lib\libcntpr.lib" /include:Mt8167RegisterHeadphoneWaveRt /include:a04_codec_headphone_start /include:a04_codec_headphone_stop /include:a04_codec_headphone_calibration "/out:$sys"
if($LASTEXITCODE){throw 'HP component link failed'}
Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-hp1028.inf" "$out\package\audio"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& $SigntoolPath verify /pa /v $sys
if($LASTEXITCODE -or (Get-AuthenticodeSignature $sys).SignerCertificate.Thumbprint -ne $thumb){throw 'SYS verify failed'}
& $Inf2CatPath "/driver:$out\package\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
$cat="$out\package\audio\mt8167-afe-hp1028.cat"
& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
& $SigntoolPath verify /pa /v $cat
if($LASTEXITCODE -or (Get-AuthenticodeSignature $cat).SignerCertificate.Thumbprint -ne $thumb){throw 'CAT verify failed'}
foreach($f in @($sys,"$out\package\audio\mt8167-afe-hp1028.inf")){& $SigntoolPath verify /pa /v /c $cat $f;if($LASTEXITCODE){throw 'CAT membership failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 ($sys+'.pe.txt')
$files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
@{source_commit=$commit;driver_version='2026.11.8.1028';signed=$true;component_build_only=$true;deployment_ready=$false;headphone_provider_present=$false;headphone_registered=$false;runtime_calibration_acquired=$false;hardware_verified=$false;parent_built=$false;owner_changed=$false;signer_thumbprint=$thumb;audio1027_baseline=$m.source_commit;compiled_tus=3;reused_objects=$reuse;files=$files}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive -Path "$out\package\*" -DestinationPath "$out\mt8167-afe-hp1028-component-signed.zip"
'MT8167_HP1028_COMPONENT_SIGNED_NOT_DEPLOYABLE'
