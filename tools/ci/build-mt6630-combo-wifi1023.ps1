# Integrate the fixed BT transport provider with current WiFi; do not rebuild BTclient2013.
param([Parameter(Mandatory=$true)][string]$ClosureDirectory,[Parameter(Mandatory=$true)][string]$ObjectPlan,[Parameter(Mandatory=$true)][string]$ClientDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$ClosureDirectory\cache\radio-cache.json" -Raw|ConvertFrom-Json
if($m.source_cache -cne '6f1ba222f2d959d01c51fc7dc35ff51d60ebb52d'){throw 'Fixed stock1019 cache provenance required'}
foreach($f in $m.files.PSObject.Properties){if((Sha (Join-Path $ClosureDirectory $f.Name)) -cne $f.Value){throw 'Closure changed'}}
$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
if($plan.Count -ne 43){throw 'Expected43 provider objects'}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package\provider","$out\package\client"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$kit='C:\mt8167-public-fullsource323\wdk81\Include';$wdk="$ClosureDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$kit\km",'-isystem',"$kit\shared",'-isystem',"$kit\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$ndis=@('-I'+"$ClosureDirectory\overlay")+$flags+@('-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$objects=@();$compiled=@();$reused=@();$records=@()
foreach($x in $plan){
 foreach($f in $x.closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Current source include closure changed'}}
 if($x.compile){
  $obj=Join-Path "$out\obj" $x.object;New-Item -Force -ItemType Directory (Split-Path $obj)|Out-Null
  $args=if($x.flags -eq 'ndis'){$ndis}else{$flags}
  & clang @args -c "$root\$($x.source)" -o $obj;if($LASTEXITCODE){throw "Compile failed $($x.source)"};$compiled+=$x.source
 }else{
  $prior=@($m.objects|Where-Object{$_.object -ceq $x.object});if($prior.Count -ne 1){throw 'Cached object identity missing'}
  foreach($f in $prior[0].verified_source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value.sha256){throw 'Reused source closure changed'}}
  $obj=Join-Path "$ClosureDirectory\cache" $x.object
  if((Sha $obj) -cne $x.cached_sha256 -or (Sha $obj) -cne $prior[0].sha256.ToLowerInvariant()){throw 'Cached object hash changed'};$reused+=$x.source
 }
 $objects+=$obj;$records+=@{source=$x.source;object=$obj;sha256=(Sha $obj);compiled=$x.compile;source_closure=$x.closure}
}
if($compiled.Count -ne 22 -or $reused.Count -ne 21){throw 'Expected22 source-dependent compiles/21reuse'}
$dir="$out\package\provider";$sys="$dir\mt6630-combo-runtime.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Provider link failed'}
Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" $dir
$clientPins=@{'mt6630-combo-bluetooth.sys'='c74992e44e4c92c0a1b301eefd36fc620846e3ba0888392b20c02e2948d70426';'mt6630-combo-bluetooth.inf'='ae09dad2188528fb1d0818afba5eb4de747bfbe8ebb08d63e3cf60dc6135ddc6';'mt6630-combo-bluetooth.cat'='4c8c06e754a93e727c55edf3b71791435ce3e8dbbe88414202ea9eec17023d6e'}
foreach($n in $clientPins.Keys){$p=Join-Path $ClientDirectory $n;if((Sha $p) -cne $clientPins[$n]){throw 'Frozen client2013 hash mismatch'};Copy-Item $p "$out\package\client"}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$cat="$dir\mt6630-combo-runtime.cat";$unsigned=Sha $sys
& $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'Provider SYS sign failed'}
& "$ClosureDirectory\signing\bin\Inf2Cat.exe" "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Provider CAT generation failed'}
& $sign sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'Provider CAT sign failed'}
foreach($p in @($sys,$cat)){& $sign verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}}
foreach($p in @($sys,"$dir\mt6630-combo-runtime.inf")){& $sign verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'Provider CAT membership failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys;if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'};$pe|Set-Content -Encoding UTF8 "$out\provider-pe.txt"
@{schema='mt6630-combo-wifi1023';source_commit=$commit;wifi_baseline='0ea7a7a91d1097ce90383516469617d44ebb6fee';transport_source='f27cbdd700cd22b2923a0e7780329d83be562ed9';version='2026.11.8.1023';checkpoint=1023;compiled=$compiled;reused=$reused;objects=$records;flags=$flags;ndis_flags=$ndis;unsigned_sys_sha256=$unsigned;signer_thumbprint=$thumb;client2013_byte_unchanged=$true;parent_rebuilt=$false;audio_rebuilt=$false;sdk_rebuilt=$false;hardware_verified=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\wifi1023-signed.zip"
Compress-Archive "$out\obj\*","$out\provider-pe.txt" "$out\wifi1023-objects.zip"
Get-FileHash "$out\wifi1023-signed.zip","$out\wifi1023-objects.zip" -Algorithm SHA256
'MT6630_WIFI1023_COMBO_SIGNED_NOT_DEVICE_TESTED'
