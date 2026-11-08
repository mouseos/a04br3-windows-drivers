# GPIO19 stock資格＋単一親D0 leaseの1TUのみ。audio1030/thermal1104不変更。
param([Parameter(Mandatory=$true)][string]$ParentInputDirectory,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$base='607a10d21552f3d157e1b369b8ad1f62843fd69e'
$drift=@(& git -C $root diff --name-only $base HEAD -- drivers)
if($LASTEXITCODE -or @($drift|Where-Object{$_ -notmatch '^drivers/(usb/mt8167-usb20/|bus/mt8167-shared/(wdf_parent_probe.c|usb_owner.[ch]|musb_[^/]+|usbport_child.h|prototype/shared-parent-usb1114.inf))$' -and $_ -notmatch '^drivers/usb/mt8167-usb20/'}).Count){throw 'Unrelated driver drift'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Check-Closure($source,$base){
 $todo=[Collections.Generic.Queue[string]]::new();$todo.Enqueue([IO.Path]::GetFullPath($source));$seen=@{}
 while($todo.Count){
  $path=$todo.Dequeue();if($seen.ContainsKey($path)){continue};$seen[$path]=$true
  if(!$path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Local include escaped repo'}
  $rel=$path.Substring($root.Length+1).Replace('\','/')
  $old=(& git -C $root rev-parse "$($base):$rel");if($LASTEXITCODE){throw 'Missing baseline dependency'}
  $new=(& git -C $root rev-parse "HEAD:$rel");if($LASTEXITCODE -or $old -cne $new){throw "Reused dependency changed $rel"}
  foreach($m in [regex]::Matches((Get-Content $path -Raw),'(?m)^\s*#\s*include\s*"([^"]+)"')){
   $dep=[IO.Path]::GetFullPath((Join-Path (Split-Path $path) $m.Groups[1].Value))
   if(!(Test-Path $dep)){throw "Missing local include $dep"};$todo.Enqueue($dep)
  }
 }
 return @($seen.Keys|Sort-Object|ForEach-Object{$_.Substring($root.Length+1).Replace('\','/')})
}

$cache=Get-Content "$ParentInputDirectory\cache\parent-cache.json" -Raw|ConvertFrom-Json
if($cache.source_commit -cne '1d248a71a95360966ff867e4b063504ac764e074'){throw 'Fixed13 helper cache required'}
if(((& clang --version)[0]) -cne (($cache.compiler -split "`n")[0])){throw 'Compiler version drift'}
$inc=$KitIncludeDirectory;$wdk="$ParentInputDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$wdk\Lib\wdf\kmdf\arm\1.13"
$sdk=@()
foreach($prop in $cache.sdk.PSObject.Properties){
 $old=$prop.Name;$p=$null
 if($old -match '\\Include\\(.+)$'){$tail=$Matches[1];$p=if($tail.StartsWith('wdf\')){"$wdk\Include\$tail"}else{"$inc\$tail"}}
 elseif($old -match '\\Lib\\(.+)$'){$p="$wdk\Lib\$($Matches[1])"}
 if($p -and (Test-Path $p)){if((Sha $p) -cne $prop.Value.ToLowerInvariant()){throw 'SDK cache mismatch'};$sdk+=@{path=$p;sha256=(Sha $p)}}
}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\parent"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$objects=@();$reused=@()
foreach($x in $cache.objects){
 if($x.name -eq 'parent'){continue};$p="$ParentInputDirectory\cache\obj\$($x.name).obj"
 if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Parent object mismatch'}
 if($x.name -eq 'usb_owner'){continue} # changed locked primitives recompiled below

 $source=if($x.name -eq 'pmic_read'){"$root\drivers\input\a04br3-buttons\pmic_read.c"}elseif($x.name -eq 'gpio22'){"$root\drivers\usb\mt8167-usb20\gpio22.c"}elseif($x.name -eq 'sd_power_owner'){"$root\drivers\storage\mt8167-sd\power_owner.c"}else{"$root\drivers\bus\mt8167-shared\$($x.name).c"}
 $deps=Check-Closure $source $cache.source_commit;$objects+=$p;$reused+=@{name=$x.name;sha256=(Sha $p);dependencies=$deps;source_commit=$cache.source_commit}
}
if($objects.Count -ne 12){throw '12 unchanged helper objects required'}
$flags=@($cache.flags)+@('-DA04_SHARED_ACCDET_READ=1','-DA04_SHARED_HP_PCM=1','-DA04_SHARED_USBPORT_CHILD=1','-DA04_USB_GPIO22_SOURCE_EXPERIMENT=1','-DMT6392_RTC_CALENDAR=1')
$kernel=@('-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$inc\km",'-isystem',"$inc\shared",'-isystem',"$inc\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$usbObj="$out\obj\usb_owner.obj"
& clang @($cache.common) @kernel @flags -w -c "$root\drivers\bus\mt8167-shared\usb_owner.c" -o $usbObj
if($LASTEXITCODE){throw 'Production USB owner compile failed'}
$objects+=$usbObj
$obj="$out\obj\parent.obj"
& clang @($cache.common) @kernel @flags -w -c "$root\drivers\bus\mt8167-shared\wdf_parent_probe.c" -o $obj
if($LASTEXITCODE){throw 'Parent1108 compile failed'}
$sys="$out\package\parent\mt8167-shared.sys";$objects=@($obj)+$objects
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Parent1108 link failed'}
$unsigned=Sha $sys;Copy-Item "$root\drivers\bus\mt8167-shared\prototype\shared-parent-usb1114.inf" "$out\package\parent"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& $Inf2Cat "/driver:$out\package\parent" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Inf2Cat failed'}
$cat="$out\package\parent\mt8167-shared-usb1114.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){
 & $SignTool verify /pa /v $p
 if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature mismatch'}
}
foreach($p in @($sys,"$out\package\parent\shared-parent-usb1114.inf")){
 & $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}
}
$pe=& llvm-readobj --file-headers --coff-imports $sys
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 ($sys+'.pe.txt')
@{schema='mt8167-musb-parent1114';source_commit=$commit;baseline_source=$base;parent_version='2026.11.8.1114';hp_extension_abi_build=1107;hp_extension_abi_version=2;thermal_v3_abi_build=1104;compiled_TUs=2;reused_helpers=$reused;compiler=(clang --version)-join "`n";parent_flags=$flags;kernel_flags=$kernel;sdk=$sdk;unsigned_sys_sha256=$unsigned;signer_thumbprint=$thumb;signed=$true;hardware_verified=$false;deployment_ready=$false;fabric_drain_proved=$false;calibration_producer_present=$true;latency_known=$false;audio_rebuilt=$false;thermal_rebuilt=$false;audio1030_sha256='ee2d4189326a92695e06a89a84994ec2ef63c3ff1615751ddcabecf27b9289a6';gpio19_stock_bias_preserved=$true;usb_parent_implemented=$true;rtc_calendar_coexistence=$true;first_real_5v_verified=$false;pull_or_dout_writes=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\musb-parent1114-signed.zip"
Compress-Archive "$out\obj\*" "$out\musb-parent1114-objects.zip"
Get-FileHash "$out\musb-parent1114-signed.zip","$out\musb-parent1114-objects.zip" -Algorithm SHA256
'MT8167_MUSB_PARENT1114_SIGNED_NOT_DEVICE_TESTED'
