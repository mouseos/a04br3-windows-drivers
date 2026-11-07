# 既存WDK/署名鍵を利用。専用出力のみ、デプロイ・OS更新なし。
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint,
 [string]$ReuseParentDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New dedicated output inside checkout required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
& git -C $root merge-base --is-ancestor 2d6231727e8ed72b6d7dca4148debc101ac5aeef HEAD
if($LASTEXITCODE){throw 'Installed parent1026 source ancestry required'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
$km=Join-Path $kit 'Lib\winv6.3\km\arm';$um=Join-Path $kit 'Lib\winv6.3\um\arm'
$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13';$rtc=Join-Path $root 'drivers\system\mt6392-rtc'
New-Item -ItemType Directory "$out\obj","$out\package\parent","$out\package\rtc","$out\package\client","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $common=@('--target=thumbv7-pc-windows-msvc','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none',
  '-ffreestanding','-O2','-fno-builtin','-fno-stack-protector','-fms-extensions','-std=c11')
 $kernel=@('-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13',
  '-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$inc\wdf\kmdf\1.13")
 $profile=Get-Content (Join-Path $root 'drivers\bus\mt8167-shared\prototype\capture-rtc1015-profile.json') -Raw|ConvertFrom-Json
 $flags=@();$effective=[ordered]@{}
 foreach($p in $profile.flags.PSObject.Properties){$flags+="-D$($p.Name)=$($p.Value)";$effective[$p.Name]=[string]$p.Value}
 $flags+=@('-DA04_SHARED_CAPTURE=1','-DMT6392_RTC_CALENDAR=1');$effective['A04_SHARED_CAPTURE']='1';$effective['MT6392_RTC_CALENDAR']='1'
 $sources=[ordered]@{}
 $sources['parent']='drivers/bus/mt8167-shared/wdf_parent_probe.c'
 $sources['pmic_read']='drivers/input/a04br3-buttons/pmic_read.c'
 foreach($name in @('parent_resources','pwrap_owner','register_owner','audio_owner','mt6392_classd','usb_owner','gpu_domain_owner','codec_adc_probe','codec_capture','capture_input_owner')){$sources[$name]="drivers/bus/mt8167-shared/$name.c"}
 $sources['gpio22']='drivers/usb/mt8167-usb20/gpio22.c';$sources['sd_power_owner']='drivers/storage/mt8167-sd/power_owner.c'
 $objects=@();$records=@();$cache=$null
 if($ReuseParentDirectory){
  $reuse=[IO.Path]::GetFullPath($ReuseParentDirectory)
  if(!$reuse.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cache outside checkout'}
  $cache=Get-Content (Join-Path $reuse 'parent-cache.json') -Raw|ConvertFrom-Json
  if($cache.compiler -ne ((& clang --version)-join "`n") -or
     ($cache.flags -join '|') -ne ($flags -join '|') -or
     ($cache.common -join '|') -ne ($common -join '|')){throw 'Cache compiler/flags changed'}
  & git -C $root diff --quiet $cache.source_commit HEAD -- drivers
  if($LASTEXITCODE){throw 'Parent source/include changed; cache reuse refused'}
  foreach($p in $cache.sdk.PSObject.Properties){if((Get-FileHash $p.Name -Algorithm SHA256).Hash -ne $p.Value){throw 'WDK cache input changed'}}
 }

 foreach($name in $sources.Keys){
  $source=Join-Path $root $sources[$name];$obj=Join-Path $out "obj\$name.obj"
  if($cache){
   $saved=@($cache.objects|Where-Object {$_.name -eq $name})
   $oldObj=Join-Path $reuse "obj\$name.obj"
   if($saved.Count -ne 1 -or (Get-FileHash $oldObj -Algorithm SHA256).Hash -ne $saved[0].sha256){throw 'Parent cache object hash mismatch'}
   Copy-Item $oldObj $obj
  }else{
   & clang @common @kernel @flags -w -c $source -o $obj
   if($LASTEXITCODE){throw "Parent compilation failed: $name"}
  }
  $objects+=$obj;$records+=@{path=$sources[$name];source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;object_sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 $sdk=[ordered]@{}
 foreach($path in @("$wdf\wdfdriverentry.lib","$wdf\wdfldr.lib","$km\ntoskrnl.lib","$km\hal.lib","$km\libcntpr.lib")){$sdk[$path]=(Get-FileHash $path -Algorithm SHA256).Hash}
 # Headers are immutable installed WDK inputs; pin their full tree too.
 foreach($file in Get-ChildItem $inc -Recurse -File){$sdk[$file.FullName]=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}
 [ordered]@{source_commit=$commit;compiler=((& clang --version)-join "`n");common=$common;flags=$flags;sdk=$sdk;
  objects=@(foreach($name in $sources.Keys){@{name=$name;sha256=(Get-FileHash (Join-Path $out "obj\$name.obj") -Algorithm SHA256).Hash}})}|
  ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 "$out\parent-cache.json"
 $parent="$out\package\parent\mt8167-shared.sys"
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects `
  "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$parent"
 if($LASTEXITCODE){throw 'RTC opt-in parent link failed'}
 $rtcObjects=@()
 foreach($name in @('wdf_rtc','calendar')){
  $source=Join-Path $rtc "$name.c";$obj=Join-Path $out "obj\$name.obj"
  & clang @common @kernel -Wall -Wextra -Werror -Wno-pragma-pack -c $source -o $obj
  if($LASTEXITCODE){throw "RTC compilation failed: $name"}
  $rtcObjects+=$obj;$records+=@{path="drivers/system/mt6392-rtc/$name.c";source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;object_sha256=(Get-FileHash $obj -Algorithm SHA256).Hash}
 }
 $sys="$out\package\rtc\mt6392-rtc.sys"
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @rtcObjects `
  "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$sys"
 if($LASTEXITCODE){throw 'RTC link failed'}
 & clang @common -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -isystem "$inc\um" -isystem "$inc\shared" -Wall -Wextra -Werror -Wno-pragma-pack -c "$rtc\rtc_probe.c" -o "$out\obj\rtc_probe.obj"
 if($LASTEXITCODE){throw 'RTC client compilation failed'}
 $exe="$out\package\client\mt6392-rtc-probe.exe"
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\obj\rtc_probe.obj" "$um\kernel32.lib" "/out:$exe"
 if($LASTEXITCODE){throw 'RTC client link failed'}
 Copy-Item "$rtc\mt6392-rtc.inf" "$out\package\rtc"
 Copy-Item "$rtc\mt8167-shared-rtc.inf" "$out\package\parent"
 $sign=Join-Path $kit 'bin\x86\signtool.exe';$inf2cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
 $cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
 if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signing key unavailable'}
 foreach($file in @($parent,$sys,$exe)){
  $headers=(& llvm-readobj --file-headers $file)-join "`n"
  if($LASTEXITCODE -or $headers -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT PE check failed'}
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $file
  if($LASTEXITCODE){throw 'Binary signing failed'}
 }
 foreach($dir in @("$out\package\parent","$out\package\rtc")){
  & $inf2cat "/driver:$dir" /os:6_3_ARM /uselocaltime
  if($LASTEXITCODE){throw 'Catalog generation failed'}
  $cat=@(Get-ChildItem $dir -Filter '*.cat');if($cat.Count -ne 1){throw 'One catalog required'}
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $cat[0].FullName
  if($LASTEXITCODE){throw 'Catalog signing failed'}
  $member=@(Get-ChildItem $dir -File|Where-Object {$_.Extension -in @('.sys','.inf')})
  foreach($file in $member){ & $sign verify /pa /v /c $cat[0].FullName $file.FullName;if($LASTEXITCODE){throw 'Catalog membership/signature failed'} }
 }
 $outputs=@()
 foreach($file in Get-ChildItem "$out\package" -Recurse -File){
  if($file.Extension -in @('.sys','.exe','.cat')) {
   $sig=Get-AuthenticodeSignature $file.FullName
   if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'Signature verification failed'}
  }
  $outputs+=@{path=$file.FullName.Substring($out.Length+1).Replace('\','/');sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash;bytes=$file.Length}
 }
 $inputs=@(Get-ChildItem (Join-Path $root 'drivers') -Recurse -File|Where-Object {$_.Extension -in @('.h','.inc','.c')}|ForEach-Object {@{path=$_.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 [ordered]@{schema=1;source_commit=$commit;source_dirty=$false;baseline_parent_source='2d6231727e8ed72b6d7dca4148debc101ac5aeef';
  hardware_verified=$false;device_deployed=$false;rtc_access='read-only';parent_driver_version='2026.11.8.1100';rtc_driver_version='0.0.1.0';
  compiler=((& clang --version)-join "`n");flags=$effective;codegen=$common;kernel=$kernel;compiled=$records;inputs=$inputs;outputs=$outputs;
  signer_thumbprint=$cert.Thumbprint;parent_objects_reused=[bool]$cache;build_note='Existing WDK; dedicated output and optional hash-pinned same-checkout parent cache; no SDK rebuild'}|
  ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
 Write-Host 'MT6392_RTC_SIGNED_NOT_DEVICE_TESTED'
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
