param([Parameter(Mandatory=$true)][string]$CacheDirectory,[Parameter(Mandatory=$true)][string]$ObjectPlan,[Parameter(Mandatory=$true)][string]$OverrideDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path $out){throw 'Fresh output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$cache=Get-Content "$CacheDirectory\cache-manifest.json" -Raw|ConvertFrom-Json
if($cache.source_commit -cne 'ae2e070851fdfb933416897321aec552d84e74a6' -or $cache.client_objects -ne 4 -or $cache.provider_objects -ne 43){throw 'Full final47 cache required'}
foreach($f in $cache.files){$p=Join-Path $CacheDirectory $f.path;if((Get-Item $p).Length -ne $f.bytes -or (Sha $p) -cne $f.sha256){throw 'Restored cache pin mismatch'}}
$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
if($plan.Count -ne 43){throw '43 provider objects required'}
New-Item -ItemType Directory "$out\obj\client","$out\obj\provider","$out\package\client","$out\package\provider","$out\tmp"|Out-Null;$env:TMP="$out\tmp";$env:TEMP=$env:TMP
$kit='C:\mt8167-public-fullsource323\wdk81\Include';$wdk="$CacheDirectory\small\wdk";$clientwdk="$CacheDirectory\small\client-wdk";$km="$wdk\Lib\winv6.3\km\arm";$cwdf="$clientwdk\Lib\wdf\kmdf\arm\1.13"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$kit\km",'-isystem',"$kit\shared",'-isystem',"$kit\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$clientflags=$flags+@('-isystem',"$clientwdk\Include\wdf\kmdf\1.13")
$ndis=@('-I'+"$CacheDirectory\small\overlay")+$flags+@('-D_WINSOCK_DEPRECATED_NO_WARNINGS=1','-DNDIS_MINIPORT_DRIVER','-DNDIS630_MINIPORT','-DA04_WIFI_SESSION_DIAGNOSTIC=1','-DA04_WIFI_IRQ_DIAGNOSTIC=1','-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1','-DA04_WIFI_BT_BROKER=1')
$objects=@{client=@();provider=@()};$records=@();$compiled=@();$reused=@()
foreach($x in $plan){
 foreach($f in $x.source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw ('Source closure changed '+$x.source_file)}}
 if($x.compile){$obj=Join-Path "$out\obj" $x.object;New-Item -ItemType Directory -Force (Split-Path $obj)|Out-Null;$args=if($x.role -eq 'provider'){$ndis}else{$clientflags}; & clang @args -c "$root\$($x.source_file)" -o $obj;if($LASTEXITCODE){throw ('Compile failed '+$x.source_file)};$compiled+=$x.source_file}
 else{$obj=if($x.override){Join-Path $OverrideDirectory $x.cached_object}else{Join-Path $CacheDirectory $x.cached_object};if((Sha $obj) -cne $x.cached_sha256){throw 'Reused object changed'};$reused+=$x.source_file}
 $objects[$x.role]+=$obj;$records+=@{role=$x.role;source=$x.source_file;object=$x.object;compiled=$x.compile;sha256=(Sha $obj);source_closure=$x.source_closure}
}
if($compiled.Count -ne 1 -or $reused.Count -ne 42 -or $objects.client.Count -ne 0 -or $objects.provider.Count -ne 43){throw 'Expected provider1 compile/42 reuse'}
$providerobjs=@($objects.provider)
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /opt:ref @providerobjs "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\package\provider\mt6630-combo-runtime.sys";if($LASTEXITCODE){throw 'Provider link failed'}
Copy-Item "$root\drivers\network\mt6630\mt6630-combo-runtime.inf" "$out\package\provider"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
foreach($role in @('provider')){$name='mt6630-combo-runtime';$dir="$out\package\$role";$sys="$dir\$name.sys";$unsigned=Sha $sys
 & $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
 & "$CacheDirectory\small\signing\bin\Inf2Cat.exe" "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
 & $sign sign /fd SHA256 /sha1 $thumb /s My "$dir\$name.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($e in @('sys','cat')){$sig=Get-AuthenticodeSignature "$dir\$name.$e";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF trust mismatch'}}
 foreach($e in @('sys','inf')){& $sign verify /pa /c "$dir\$name.cat" "$dir\$name.$e";if($LASTEXITCODE){throw 'CAT member failed'}}
 & llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\$role-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
 @{schema='mt6630-provider-async-pause1027';source_commit=$commit;role=$role;version='2026.11.9.1027';unsigned_sha256=$unsigned;signer_thumbprint=$thumb;signature_status='Valid';objects=@($records|Where-Object{$_.role -eq $role});flags=$(if($role -eq 'client'){$clientflags}else{$ndis});SDK_setup=$false;parent_rebuilt=$false;audio_rebuilt=$false;hardware_verified=$false;power9F_root_proven=$false;Carson_source_commit='2df626895513d734ba5af87837d9e0c5bcf81530';client2018_unchanged=$true;files=@(Get-ChildItem $dir -File|ForEach-Object{@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$dir\signed-manifest.json"
}
$records|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\object-manifest.json"
Compress-Archive "$out\package\*" "$out\signed-packages.zip";Compress-Archive "$out\obj\*","$out\object-manifest.json","$out\provider-pe.txt" "$out\changed-objects.zip"
'MT6630_PROVIDER1027_CLIENT2018_RETAINED_SIGNED_NO_DEVICE_CLAIM'
