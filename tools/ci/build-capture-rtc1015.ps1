# Capture-only integration on signed parent1015 source/flags; no deployment.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint,[string]$ReuseParentDirectory,[string]$AudioCacheManifest)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New output within this checkout required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
$baseline=Get-Content (Join-Path $root 'drivers\bus\mt8167-shared\prototype\capture-rtc1015-profile.json') -Raw|ConvertFrom-Json
$values=@{RUNNER_TEMP=$out;A04_SHARED_CAPTURE='1'}
foreach($p in $baseline.flags.PSObject.Properties){$values[$p.Name]=[string]$p.Value}
$previous=@{};foreach($k in @($values.Keys)+@('TEMP','TMP')){$previous[$k]=[Environment]::GetEnvironmentVariable($k,'Process')}
New-Item -ItemType Directory "$out\tmp","$out\audio","$out\client"|Out-Null
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($k in $values.Keys){[Environment]::SetEnvironmentVariable($k,$values[$k],'Process')}
 & git -C $root merge-base --is-ancestor $baseline.source_commit HEAD
 if($LASTEXITCODE){throw 'Real parent1015 ancestry required'}
 if($ReuseParentDirectory){
  $reuse=[IO.Path]::GetFullPath($ReuseParentDirectory)
  if(!$reuse.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Reuse parent must be in the same checkout'}
  $pm=Get-Content (Join-Path $reuse 'parent-manifest.json') -Raw|ConvertFrom-Json
  if($pm.source_dirty -or $pm.flags.A04_SHARED_CAPTURE -ne '1' -or $pm.driver_version -ne '2026.11.7.1026'){throw 'Not a fixed capture parent'}
  if((Get-FileHash (Join-Path $reuse 'mt8167-shared.sys') -Algorithm SHA256).Hash -ne $pm.sys_sha256){throw 'Reused parent hash mismatch'}
  foreach($p in $baseline.flags.PSObject.Properties){if([string]$pm.flags.($p.Name) -ne [string]$p.Value){throw 'Reused parent flag mismatch'}}
  & git -C $root diff --quiet $pm.source_commit HEAD -- drivers/bus drivers/input/a04br3-buttons/pmic_read.c drivers/usb/mt8167-usb20/gpio22.c drivers/storage/mt8167-sd/power_owner.c tools/ci/build-msdc-parent-probe.ps1
  if($LASTEXITCODE){throw 'Parent source changed; rebuild required'}
  $reusedOut=Join-Path $out 'a04-msdc-parent-probe';New-Item -ItemType Directory $reusedOut|Out-Null
  foreach($n in @('mt8167-shared.sys','shared-parent.inf','parent-manifest.json')){Copy-Item (Join-Path $reuse $n) $reusedOut}
 }else{ & (Join-Path $PSScriptRoot 'build-msdc-parent-probe.ps1') }
 $effective=Get-Content (Join-Path $out 'a04-msdc-parent-probe\parent-manifest.json') -Raw|ConvertFrom-Json
 foreach($p in $baseline.flags.PSObject.Properties){if([string]$effective.flags.($p.Name) -ne [string]$p.Value){throw 'Existing device flag changed'}}
 if($effective.flags.A04_SHARED_CAPTURE -ne '1' -or $effective.flags.A04_SHARED_WIFI_RTC_BORROW -ne '1'){throw 'Capture/borrow contract missing'}
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\km\arm'
 $audio=Join-Path $root 'drivers\audio\mt8167-afe';$obj=Join-Path $out 'audio'
 $common=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
  '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions','-w',
  '-D__ldrexd=__a04_capture_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM',
  '-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
 $flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1',
  '-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1',
  '-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_DIGITAL_PLAYBACK=1',
  '-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1',
  '-DA04_AUDIO_START_PERSIST=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_CAPTURE=1')
 . (Join-Path $PSScriptRoot 'capture-audio-cache.ps1')
 $script:AudioCompilerIdentity=((& clang --version)-join "`n");$script:AudioObjectResults=@();$script:AudioObjectCache=$null
 if($AudioCacheManifest){
  $cachePath=[IO.Path]::GetFullPath($AudioCacheManifest)
  if(!$cachePath.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Audio cache must be inside the same checkout'}
  $script:AudioObjectCache=Get-Content $cachePath -Raw|ConvertFrom-Json
  if($script:AudioObjectCache.source_commit -ne 'e0ac12f14173e62dfb4a2cb52bcd47e1f7b53fa7'){throw 'Only fixed prior capture objects accepted'}
 }
 $objects=@();$sources=@()
 foreach($name in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query','capture_parent_query','ul_engine')){
  $file=Join-Path $obj ($name+'.obj');$source=Join-Path $audio ($name+'.c');$objects+=$file;$sources+=$source
  Build-CaptureObject clang ($common+$flags) $source $file
  if($LASTEXITCODE){throw "Capture C compilation failed: $name"}
 }
 $file=Join-Path $obj 'clock_owner.obj';$source=Join-Path $root 'drivers\bus\mt8167-shared\clock_owner.c';$objects+=$file;$sources+=$source
 Build-CaptureObject clang $common $source $file
 if($LASTEXITCODE){throw 'Clock helper compilation failed'}
 foreach($name in @('adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter',
  'capture_latency_gate','stream_transport','pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','start_record','capture_owner')){
  $file=Join-Path $obj ($name+'.obj');$source=Join-Path $audio ($name+'.cpp');$objects+=$file;$sources+=$source
  Build-CaptureObject clang++ ($common+$flags+@('-std=c++14','-fms-compatibility','-fno-exceptions','-fno-rtti')) $source $file
  if($LASTEXITCODE){throw "Capture C++ compilation failed: $name"}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects `
  (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') `
  (Join-Path $lib 'bufferoverflowfastfailk.lib') (Join-Path $lib 'libcntpr.lib') "/out:$obj\mt8167-audio.sys"
 if($LASTEXITCODE){throw 'Capture driver link failed'}
 Copy-Item (Join-Path $audio 'mt8167-afe-engine-capture.inf') $obj
 $parent=Join-Path $out 'parent';New-Item -ItemType Directory $parent|Out-Null
 foreach($n in @('mt8167-shared.sys','shared-parent.inf','parent-manifest.json')){Copy-Item (Join-Path $out "a04-msdc-parent-probe\$n") $parent}
 $client=Join-Path $out 'client';$um=Join-Path $kit 'Lib\winv6.3\um\arm'
 & clang --target=thumbv7-pc-windows-msvc -O2 -w -fno-builtin -fno-vectorize -fno-slp-vectorize -mfpu=none -mno-unaligned-access -fms-extensions -ffreestanding -fno-stack-protector `
  -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\audio\mt8167-audio-capture-probe.c') -o "$client\client.obj"
 if($LASTEXITCODE){throw 'Capture KS client compilation failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$client\client.obj" `
  (Join-Path $um 'kernel32.lib') (Join-Path $um 'setupapi.lib') (Join-Path $um 'cfgmgr32.lib') "/out:$client\mt8167-capture-probe.exe"
 if($LASTEXITCODE){throw 'Capture KS client link failed'}
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
 if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Signing key unavailable'}
 $sign=Join-Path $kit 'bin\x86\signtool.exe';$inf2cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
 $package=Join-Path $out 'package';New-Item -ItemType Directory "$package\parent","$package\audio","$package\client"|Out-Null
 foreach($pair in @(@($parent,'parent','mt8167-shared.sys'),@($obj,'audio','mt8167-audio.sys'))){
  $dir=Join-Path $package $pair[1]
  Get-ChildItem $pair[0] -File|Where-Object {$_.Extension -in @('.inf','.sys','.json')}|Copy-Item -Destination $dir
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $dir $pair[2]);if($LASTEXITCODE){throw 'SYS signing failed'}
  & $inf2cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'INF catalog failed'}
  $cat=@(Get-ChildItem $dir -Filter '*.cat');if($cat.Count -ne 1){throw 'One catalog required'}
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $cat[0].FullName;if($LASTEXITCODE){throw 'CAT signing failed'}
  foreach($f in Get-ChildItem $dir -File|Where-Object {$_.Extension -in @('.sys','.cat')}){
   & $sign verify /pa /v $f.FullName;if($LASTEXITCODE){throw 'Signature verify failed'}
   if((Get-AuthenticodeSignature $f.FullName).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signer mismatch'}
  }
  foreach($f in Get-ChildItem $dir -File|Where-Object {$_.Extension -in @('.sys','.inf')}){
   & $sign verify /pa /v /c $cat[0].FullName $f.FullName;if($LASTEXITCODE){throw 'CAT membership failed'}
  }
 }
 $pmPath=Join-Path $package 'parent\parent-manifest.json'
 $pm=Get-Content $pmPath -Raw|ConvertFrom-Json
 $pm|Add-Member -NotePropertyName unsigned_sys_sha256 -NotePropertyValue $pm.sys_sha256 -Force
 $pm.sys_sha256=(Get-FileHash "$package\parent\mt8167-shared.sys" -Algorithm SHA256).Hash
 $pm|Add-Member -NotePropertyName signed -NotePropertyValue $true -Force
 $pm|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $pmPath
 Copy-Item "$client\mt8167-capture-probe.exe" "$package\client"
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My "$package\client\mt8167-capture-probe.exe"
 if($LASTEXITCODE){throw 'Client signing failed'}
 & $sign verify /pa /v "$package\client\mt8167-capture-probe.exe";if($LASTEXITCODE){throw 'Client verification failed'}
 foreach($f in Get-ChildItem $package -Recurse -File|Where-Object {$_.Extension -in @('.sys','.exe')}){
  $pe=& llvm-readobj --file-headers $f.FullName
  if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 }
 Export-Certificate -Cert $cert -FilePath "$package\development.cer"|Out-Null
 [ordered]@{source_commit=$commit;source_dirty=$false;build_flags=$flags;parent_flags=$values;signer=$cert.Thumbprint;
  runtime_experiment=$true;production_candidate=$false;capture_parent_interface=1;audio_version='2026.11.7.1026';parent_version='2026.11.7.1026';
  baseline_profile='signed RTC borrow parent1015';current_rtc1015_integrated=$true;baseline_source_commit=$baseline.source_commit;baseline_signed_sys_sha256=$baseline.signed_sys_sha256;
  installed_parent_replacement_allowed=$false;existing_parent_flags_verified_unchanged=$true;added_parent_flags=@('A04_SHARED_CAPTURE');usb_leaf_rebuilt=$false;usb1005_source_integrated=$false;hardware_verified=$false;microphone_recording_verified=$false;latency_known=$false;dma_drain_certified=$false;
  sources=@($sources|ForEach-Object {@{path=$_.Substring($root.Length+1);sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}});
  incremental_audio_reused=@($script:AudioObjectResults|Where-Object {$_.reused}).Count;incremental_audio_compiled=@($script:AudioObjectResults|Where-Object {!$_.reused}).Count;
  files=@(Get-ChildItem $package -Recurse -File|ForEach-Object {@{path=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$package\signed-manifest.json"
 @{compiler=$script:AudioCompilerIdentity;prior_source='e0ac12f14173e62dfb4a2cb52bcd47e1f7b53fa7';source_commit=$commit;units=$script:AudioObjectResults}|ConvertTo-Json -Depth 9|Set-Content -Encoding UTF8 "$out\audio-incremental-manifest.json"
 Compress-Archive -Path "$package\*" -DestinationPath "$out\mt8167-capture-rtc1026-signed.zip"
 'CAPTURE_RTC1026_SYS_CAT_CLIENT_COMPLETE'
}finally{foreach($k in $previous.Keys){[Environment]::SetEnvironmentVariable($k,$previous[$k],'Process')}}
