# MT6630 BTHX clientのみ1TU増分。既存SDIO provider/Audio/親は再buildしない。
param([Parameter(Mandatory=$true)][string]$ClosureDirectory,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$ClosureDirectory\cache\radio-cache.json" -Raw|ConvertFrom-Json
if($m.source_cache -cne '6f1ba222f2d959d01c51fc7dc35ff51d60ebb52d' -or $m.objects.Count -ne 44){throw 'Fixed stock1019 cache required'}
foreach($f in $m.files.PSObject.Properties){if((Sha (Join-Path $ClosureDirectory $f.Name)) -cne $f.Value){throw 'Closure bytes changed'}}
$provider=@();$client=@();$reused=@()
foreach($x in $m.objects){
 foreach($f in $x.verified_source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value.sha256){throw 'Reused object source closure changed'}}
 $p=Join-Path "$ClosureDirectory\cache" $x.object;if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Reused object changed'};if($x.object.StartsWith('core/') -or $x.object.StartsWith('ndis/') -or $x.object -eq 'bt-transport.obj'){$provider+=$p}else{$client+=$p};$reused+=$x
}
New-Item -ItemType Directory "$out\obj","$out\package\client","$out\package\provider","$out\tmp"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$wdk="$ClosureDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$wdk\Lib\wdf\kmdf\arm\1.13"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$KitIncludeDirectory\km",'-isystem',"$KitIncludeDirectory\shared",'-isystem',"$KitIncludeDirectory\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$obj="$out\obj\combo_client.obj"
& clang @flags -c "$root\drivers\bluetooth\mt6630\combo_client.c" -o $obj;if($LASTEXITCODE){throw 'Client compile failed'}
$client=@($obj)+$client
$ndis=@('-I'+"$ClosureDirectory\overlay")+$flags+@('-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
foreach($name in @('ndis_runtime','combo_broker')){
 $obj="$out\obj\$name.obj"
 & clang @ndis -c "$root\drivers\network\mt6630\$name.c" -o $obj;if($LASTEXITCODE){throw "Provider compile failed $name"};$provider+=$obj
}
if($provider.Count -ne 43 -or $client.Count -ne 4){throw 'Expected source-bound object graph'}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref @client "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\package\client\mt6630-combo-bluetooth.sys"
if($LASTEXITCODE){throw 'Client link failed'}
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @provider "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\package\provider\mt6630-combo-runtime.sys"
if($LASTEXITCODE){throw 'Provider link failed'}
Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\package\client"
Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\package\provider"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
$unsigned=@()
foreach($kind in @('client','provider')){
 $name=if($kind -eq 'client'){'mt6630-combo-bluetooth'}else{'mt6630-combo-runtime'}
 $dir="$out\package\$kind";$sys="$dir\$name.sys";$cat="$dir\$name.cat"
 $unsigned+=@{path="$kind/$name.sys";sha256=(Sha $sys)}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
 & "$ClosureDirectory\signing\bin\Inf2Cat.exe" "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($p in @($sys,$cat)){& $SignTool verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}}
 foreach($p in @($sys,"$dir\$name.inf")){& $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
 & llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\$kind-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
}
@{schema='mt6630-radio-transport-split';source_commit=$commit;client_version='2026.11.8.2013';provider_version='2026.11.8.1020';compiled=@('combo_client','ndis_runtime','combo_broker');reused=$reused;flags=$flags;ndis_flags=$ndis;compiler=(& clang --version);unsigned=$unsigned;signer_thumbprint=$thumb;transport_caps='live stock Merge/FC72 ACK route, independent of Audio qualification';HFP_gate_unchanged=$true;parent_delta_required=$false;audio_rebuilt=$false;hardware_verified=$false;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\radio-signed.zip"
Compress-Archive "$out\obj\*","$out\client-pe.txt","$out\provider-pe.txt" "$out\radio-objects.zip"
Get-FileHash "$out\radio-signed.zip","$out\radio-objects.zip" -Algorithm SHA256
