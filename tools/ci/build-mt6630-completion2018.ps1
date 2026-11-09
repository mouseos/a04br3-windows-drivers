param([Parameter(Mandatory=$true)][string]$CacheDirectory,[Parameter(Mandatory=$true)][string]$ObjectPlan,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path $out){throw 'New output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$plan=Get-Content $ObjectPlan -Raw|ConvertFrom-Json
if($plan.Count -ne 4 -or @($plan|Where-Object{$_.compile}).Count -ne 1){throw 'Client4: one TU/three cached required'}
New-Item -ItemType Directory "$out\obj","$out\package","$out\tmp"|Out-Null;$env:TMP="$out\tmp";$env:TEMP=$env:TMP
$kit='C:\mt8167-public-fullsource323\wdk81\Include';$wdk="$CacheDirectory\small\wdk";$cw="$CacheDirectory\small\client-wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$cw\Lib\wdf\kmdf\arm\1.13"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$kit\km",'-isystem',"$kit\shared",'-isystem',"$kit\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13",'-isystem',"$cw\Include\wdf\kmdf\1.13")
$objects=@();$records=@()
foreach($x in $plan){foreach($f in $x.source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Source closure mismatch'}}
 if($x.compile){$obj=Join-Path "$out\obj" ([IO.Path]::GetFileName($x.object));& clang @flags -c "$root\$($x.source_file)" -o $obj;if($LASTEXITCODE){throw 'Client compile failed'}}else{$obj=Join-Path $CacheDirectory $x.cached_object;if((Sha $obj) -cne $x.cached_sha256){throw 'Cached object mismatch'}}
 $objects+=$obj;$records+=@{source=$x.source_file;object=$x.object;compiled=$x.compile;sha256=(Sha $obj);source_closure=$x.source_closure}
}
$dir="$out\package";$name='mt6630-combo-bluetooth';$sys="$dir\$name.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$sys";if($LASTEXITCODE){throw 'Client link failed'}
Copy-Item "$root\drivers\bluetooth\mt6630\$name.inf" $dir;$unsigned=Sha $sys
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& "$CacheDirectory\small\signing\bin\Inf2Cat.exe" "/driver:$dir" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
& $sign sign /fd SHA256 /sha1 $thumb /s My "$dir\$name.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($e in @('sys','cat')){$s=Get-AuthenticodeSignature "$dir\$name.$e";if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -cne $thumb){throw 'BCBF mismatch'}}
foreach($e in @('sys','inf')){& $sign verify /pa /c "$dir\$name.cat" "$dir\$name.$e";if($LASTEXITCODE){throw 'CAT member mismatch'}}
@{schema='mt6630-client-completion2018';source_commit=$commit;version='2026.11.9.2018';unsigned_sha256=$unsigned;signer_thumbprint=$thumb;signature_status='Valid';compiled_TUs=1;reused_objects=3;objects=$records;provider_unchanged='1026';broker_ABI_changed=$false;hardware_verified=$false;radio_verified=$false;power9F_root_proven=$false;files=@(Get-ChildItem $dir -File|ForEach-Object{@{path=$_.Name;bytes=$_.Length;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$dir\signed-manifest.json"
$records|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\object-manifest.json"
Compress-Archive "$dir\*" "$out\signed-client.zip";Compress-Archive "$out\obj\*","$out\object-manifest.json" "$out\changed-objects.zip"
'MT6630_COMPLETION2018_SIGNED_NO_HARDWARE_CLAIM'
