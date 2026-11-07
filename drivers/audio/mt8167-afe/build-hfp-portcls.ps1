# Fixed source composition only. Never operates a runner, target, hive or media.
param([Parameter(Mandatory=$true)][string]$MatchedParentManifest,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [switch]$EnableHfpCandidate,[switch]$Sign,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
if(!$EnableHfpCandidate){throw 'HFP source candidate requires explicit EnableHfpCandidate'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Use a new output inside this checkout'}
$profile=Get-Content -LiteralPath $MatchedParentManifest -Raw|ConvertFrom-Json
if($profile.source_dirty -ne $false -or $profile.flags.A04_SHARED_ENGINE_PCM -ne '1' -or
   $profile.flags.A04_SHARED_USB_ONLY -ne '0' -or $profile.flags.A04_SHARED_TOUCH_D0_EXPERIMENT -ne '1' -or
   $profile.flags.A04_SHARED_BUTTON_D0_EXPERIMENT -ne '1' -or $profile.flags.A04_SHARED_WIFI_RUNTIME -ne '1'){
 throw 'An actual matched full Engine parent profile is required'
}
$btCommit='63c32fdc829a8ba65152a49e30ffdc8a1131ae17'
& git -C $root cat-file -e ($btCommit+'^{commit}') 2>$null
if($LASTEXITCODE){& git -C $root fetch --no-tags origin $btCommit;if($LASTEXITCODE){throw 'Pinned BT dependency fetch failed'}}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp'),(Join-Path $out 'parent'),(Join-Path $out 'audio')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$oldRunner=$env:RUNNER_TEMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$env:TEMP
 $archive=Join-Path $out 'bt-source.zip';$dependency=Join-Path $out 'dependency'
 & git -C $root archive --format=zip "--output=$archive" $btCommit drivers/bluetooth/mt6630
 if($LASTEXITCODE){throw 'Pinned BT source archive failed'}
 Expand-Archive -LiteralPath $archive -DestinationPath $dependency
 $bt=Join-Path $dependency 'drivers\bluetooth\mt6630'
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm'
 $wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13';$audio=$PSScriptRoot;$bus=Join-Path $root 'drivers\bus\mt8167-shared'
 $common=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector',
  '-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions',
  '-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',
  "-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13","-I$bt")
 $parentFlags=@();foreach($p in $profile.flags.PSObject.Properties){
  if($p.Name -notmatch '^A04_[A-Z0-9_]+$' -or [string]$p.Value -notmatch '^[01]$'){throw 'Parent flags must be fixed Boolean A04 source flags'}
  $parentFlags+="-D$($p.Name)=$($p.Value)"
 }
 $parentFlags=@($parentFlags|Where-Object {$_ -notmatch '^-DA04_SHARED_(HFP|BT_PROFILE)='})+@('-DA04_SHARED_HFP=1','-DA04_SHARED_BT_PROFILE=1','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13')
 $parentObjects=@();$parentOut=Join-Path $out 'parent'
 foreach($part in @('wdf_parent_probe','codec_adc_probe','parent_resources','pwrap_owner','register_owner','audio_owner','mt6392_classd','usb_owner')){
  $obj=Join-Path $parentOut ($part+'.obj');$parentObjects+=$obj
  & clang @common @parentFlags -w -c (Join-Path $bus ($part+'.c')) -o $obj
  if($LASTEXITCODE){throw "HFP parent compile failed: $part"}
 }
 foreach($part in @(@('pmic_read','drivers\input\a04br3-buttons\pmic_read.c'),@('gpio22','drivers\usb\mt8167-usb20\gpio22.c'))){
  $obj=Join-Path $parentOut ($part[0]+'.obj');$parentObjects+=$obj
  & clang @common @parentFlags -w -c (Join-Path $root $part[1]) -o $obj
  if($LASTEXITCODE){throw "HFP parent core compile failed: $($part[0])"}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @parentObjects `
  (Join-Path $wdf 'wdfdriverentry.lib') (Join-Path $wdf 'wdfldr.lib') (Join-Path $km 'ntoskrnl.lib') `
  (Join-Path $km 'hal.lib') (Join-Path $km 'bufferoverflowfastfailk.lib') (Join-Path $km 'libcntpr.lib') "/out:$parentOut\mt8167-shared.sys"
 if($LASTEXITCODE){throw 'HFP parent link failed'}
 Copy-Item (Join-Path $bus 'prototype\shared-parent-engine-hfp.inf') (Join-Path $parentOut 'shared-parent.inf')
 $flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1','-DA04_MT8167_OWNER_DIAGNOSTIC=1',
  '-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1','-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1',
  '-DA04_AUDIO_ENGINE_PCM=1','-DA04_AUDIO_VOICE_PCM_DIAGNOSTIC=1','-DA04_AUDIO_WAVE_DMA_RETAINED=1',
  '-DA04_AUDIO_DIGITAL_PLAYBACK=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1','-DA04_AUDIO_HFP_PROVIDER=1')
 $audioOut=Join-Path $out 'audio';$objects=@()
 foreach($part in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query','hfp_memif')){
  $obj=Join-Path $audioOut ($part+'.obj');$objects+=$obj
  & clang @common @flags -w -c (Join-Path $audio ($part+'.c')) -o $obj
  if($LASTEXITCODE){throw "Audio C compile failed: $part"}
 }
 $obj=Join-Path $audioOut 'clock_owner.obj';$objects+=$obj
 & clang @common @flags -w -c (Join-Path $bus 'clock_owner.c') -o $obj
 if($LASTEXITCODE){throw 'Audio clock owner compile failed'}
 foreach($part in @('adapter','wavert_miniport','wavert_registration','wavert_dma','common_dma','dma_adapter','stream_transport',
  'pnp_backend','afe_mapping','irq_windows','wave_dma_live','topology_miniport','hfp_provider','hfp_power')){
  $obj=Join-Path $audioOut ($part+'.obj');$objects+=$obj
  & clang++ @common @flags -std=c++14 -fno-exceptions -fno-rtti -w -c (Join-Path $audio ($part+'.cpp')) -o $obj
  if($LASTEXITCODE){throw "Audio C++ compile failed: $part"}
 }
 foreach($part in @('sideband','hfp_endpoint','hfp_pnp','hfp_topology','ks_endpoints')){
  $cpp=$part -in @('hfp_pnp','hfp_topology','ks_endpoints');$ext=if($cpp){'.cpp'}else{'.c'}
  $obj=Join-Path $audioOut ('bt-'+$part+'.obj');$objects+=$obj
  if($cpp){& clang++ @common @flags -std=c++14 -fno-exceptions -fno-rtti -w -c (Join-Path $bt ($part+$ext)) -o $obj}
  else {& clang @common @flags -w -c (Join-Path $bt ($part+$ext)) -o $obj}
  if($LASTEXITCODE){throw "Pinned BT endpoint compile failed: $part"}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects `
  (Join-Path $km 'portcls.lib') (Join-Path $km 'ntoskrnl.lib') (Join-Path $km 'hal.lib') `
  (Join-Path $km 'bufferoverflowfastfailk.lib') (Join-Path $km 'libcntpr.lib') "/out:$audioOut\mt8167-audio.sys"
 if($LASTEXITCODE){throw 'Audio HFP PortCls link failed'}
 Copy-Item (Join-Path $audio 'mt8167-afe-engine-hfp.inf') $audioOut
 if($Sign){
  if($CertificateThumbprint -notmatch '^[0-9a-fA-F]{40}$'){throw 'Signing requires an explicit existing certificate'}
  $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
  if(!$cert.HasPrivateKey -or [DateTime]::UtcNow -lt $cert.NotBefore.ToUniversalTime() -or [DateTime]::UtcNow -gt $cert.NotAfter.ToUniversalTime()){throw 'Valid signing private key required'}
  $sign=Join-Path $kit 'bin\x86\signtool.exe';$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
  foreach($dir in @($parentOut,$audioOut)){
   $sys=Get-ChildItem $dir -Filter '*.sys';if(@($sys).Count -ne 1){throw 'One fixed SYS per component required'}
   & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $sys.FullName;if($LASTEXITCODE){throw 'SYS signing failed'}
   & $cat "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'HFP Inf2Cat failed'}
   $catalog=Get-ChildItem $dir -Filter '*.cat';if(@($catalog).Count -ne 1){throw 'One fixed CAT per component required'}
   & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $catalog.FullName;if($LASTEXITCODE){throw 'CAT signing failed'}
   & $sign verify /pa /v /c $catalog.FullName $sys.FullName;if($LASTEXITCODE){throw 'CAT membership verification failed'}
  }
  [IO.File]::WriteAllBytes((Join-Path $out 'mt8167-development.cer'),$cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert))
 }
 [ordered]@{source_commit=(& git -C $root rev-parse HEAD);source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);
  bt_dependency_source_commit=$btCommit;matched_parent_manifest_sha256=(Get-FileHash $MatchedParentManifest -Algorithm SHA256).Hash;
  parent_profile_flags=$parentFlags;audio_flags=$flags;portcls_slots=6;hfp_slots=4;bt_wlan_exclusive=$true;
  signed=[bool]$Sign;signing_thumbprint=$(if($Sign){$cert.Thumbprint}else{$null});
  hfp_factory_requires_actual_dma_drain_witness=$true;independent_dma_drain_implemented=$false;
  controller_registration_implemented_here=$false;pcm_controller_format_verified=$false;hci_bypass_capability_advertised=$false;
  parent_driver_ver='11/06/2026,2026.11.7.1007';audio_driver_ver='11/06/2026,2026.11.7.1005';
  hardware_verified=$false;status='source candidate; HFP admission remains unavailable without actual controller and drain witness';
  files=@(Get-ChildItem $parentOut,$audioOut -File|Where-Object {$_.Extension -in '.sys','.inf','.cat'}|ForEach-Object {@{path=$_.FullName.Substring($out.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 (Join-Path $out 'hfp-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp;$env:RUNNER_TEMP=$oldRunner}
