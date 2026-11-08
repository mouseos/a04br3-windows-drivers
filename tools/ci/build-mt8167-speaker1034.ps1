param([Parameter(Mandatory=$true)][string]$Inputs,[Parameter(Mandatory=$true)][string]$Audio1033,[Parameter(Mandatory=$true)][string]$ObjectPlan,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$Inputs\closure-manifest.json" -Raw|ConvertFrom-Json
foreach($f in $m.files){if((Sha (Join-Path $Inputs $f.path)) -cne $f.sha256){throw 'Small closure changed'}}
$cache=Get-Content "$Inputs\parent-cache.json" -Raw|ConvertFrom-Json
if(((& clang --version)[0]) -cne (($cache.compiler -split "`n")[0])){throw 'Compiler drift'}
$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
if($plan.Count -ne 23){throw 'Expected23 audio objects; blanket gate removed'}
$kit='C:\mt8167-public-fullsource323\wdk81\Include';$common=@($cache.common|Where-Object{$_ -ne '-std=c11'})
$flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1','-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1','-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_DIGITAL_PLAYBACK=1','-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1','-DA04_AUDIO_START_PERSIST=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_CAPTURE=1','-DA04_AUDIO_ACCDET_JACK=1','-DA04_AUDIO_HP_COMPONENT=1','-DA04_AUDIO_HP_SHARED=1')
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$kit\km",'-isystem',"$kit\shared",'-isystem',"$kit\um")
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\audio","$out\package\clients"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$objects=@();$compiled=@();$reused=@();$records=@()
foreach($x in $plan){
 foreach($f in $x.closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Current source closure changed'}}
 if($x.compile){
  $obj="$out\obj\$($x.name).obj"
  & clang @common @kernel @flags -std=c++14 -fms-compatibility -fno-exceptions -fno-rtti -w -c "$root\$($x.source)" -o $obj
  if($LASTEXITCODE){throw "Compile failed $($x.name)"};$compiled+=$x.name
 }else{
  $obj=if($x.location -eq 'audio1033'){"$Audio1033\obj\$($x.name).obj"}else{"$Inputs\obj\$($x.name).obj"}
  if((Sha $obj) -cne $x.cached_sha256){throw 'Cached object hash changed'}
  foreach($f in $x.closure.PSObject.Properties){
   $blob=& git -C $root rev-parse "$($x.baseline_commit):$($f.Name)";if($LASTEXITCODE){throw 'Baseline source absent'}
   $current=& git -C $root rev-parse "HEAD:$($f.Name)";if($LASTEXITCODE -or $blob -cne $current){throw 'Reused source closure differs'}
  };$reused+=$x.name
 }
 $objects+=$obj;$records+=@{name=$x.name;source=$x.source;object=$obj;sha256=(Sha $obj);compiled=$x.compile}
}
if($compiled.Count -ne 4 -or $reused.Count -ne 19){throw 'Expected4TU incremental/19reuse'}
$dir="$out\package\audio";$sys="$dir\mt8167-audio.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects "$Inputs\portcls.lib" "$Inputs\km\ntoskrnl.lib" "$Inputs\km\hal.lib" "$Inputs\km\bufferoverflowfastfailk.lib" "$Inputs\km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Audio link failed'}
Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-speaker1034.inf" $dir
$client="$Audio1033\package\clients\mt8167-silent-pcm-consumer.exe"
if((Sha $client) -cne '886bb9f62aebf8286157f3dd0a3e745217936b8bf0bceac79ef0dd5106487770'){throw 'Frozen1033 consumer mismatch'}
Copy-Item $client "$out\package\clients"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$unsigned=Sha $sys
& $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& "$Inputs\signing\bin\Inf2Cat.exe" "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$dir\mt8167-afe-speaker1034.cat"
& $sign sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){& $sign verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}}
foreach($p in @($sys,"$dir\mt8167-afe-speaker1034.inf")){& $sign verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys;if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'};$pe|Set-Content -Encoding UTF8 "$out\audio-pe.txt"
@{schema='mt8167-speaker1034';source_commit=$commit;version='2026.11.8.1034';compiled=$compiled;reused=$reused;objects=$records;audio_flags=$flags;compiler_flags=$common;kernel_flags=$kernel;unsigned_sys_sha256=$unsigned;signer_thumbprint=$thumb;consumer1033_byte_unchanged=$true;hp_qi_version=2;hp_qi_build=1107;thermal_qi_build=1104;parent_rebuilt=$false;sdk_rebuilt=$false;hwlatency_basis='CT3 native48k speaker vendor-reported nominal zero FIFO/extra delay; not measured or guaranteed zero physical latency';hwlatency_precision_verified=$false;hardware_verified=$false;audible_verified=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\speaker1034-signed.zip"
Compress-Archive "$out\obj\*","$out\audio-pe.txt" "$out\speaker1034-objects.zip"
Get-FileHash "$out\speaker1034-signed.zip","$out\speaker1034-objects.zip" -Algorithm SHA256
'MT8167_SPEAKER1034_NOMINAL_SIGNED_NOT_DEVICE_TESTED'
