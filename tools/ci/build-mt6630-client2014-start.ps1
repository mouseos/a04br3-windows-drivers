# MT6630 client2014:2 changed TUs,2 source-verified objects reused. No provider/Audio/parent build.
param([Parameter(Mandatory=$true)][string]$ClosureDirectory,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$ClosureDirectory\cache\client-cache.json" -Raw|ConvertFrom-Json
if($m.source_cache -cne '677c756958d29d3d09274d8256c8024023d9a1c7' -or $m.objects.Count -ne 3){throw 'Fixed 3-object client cache required'}
foreach($f in $m.files.PSObject.Properties){if((Sha (Join-Path $ClosureDirectory $f.Name)) -cne $f.Value){throw 'Closure bytes changed'}}
$objects=@();$reused=@()
foreach($x in @($m.objects|Where-Object {$_.object -cne 'combo_controller.obj'})){
 foreach($f in $x.verified_source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Reused object source closure changed'}}
 $p=Join-Path "$ClosureDirectory\cache" $x.object;if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Reused object changed'};$objects+=$p;$reused+=$x
}
New-Item -ItemType Directory "$out\obj","$out\package","$out\tmp"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$wdk="$ClosureDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$wdk\Lib\wdf\kmdf\arm\1.13"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$KitIncludeDirectory\km",'-isystem',"$KitIncludeDirectory\shared",'-isystem',"$KitIncludeDirectory\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$compiled=@()
foreach($unit in @('combo_client','combo_controller')){
 $obj="$out\obj\$unit.obj"
 & clang @flags -c "$root\drivers\bluetooth\mt6630\$unit.c" -o $obj;if($LASTEXITCODE){throw ('Client TU compile failed '+$unit)}
 $compiled+=$obj
}
$objects=$compiled+$objects;$sys="$out\package\mt6630-combo-bluetooth.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Client link failed'}
Copy-Item "$root\drivers\bluetooth\mt6630\mt6630-combo-bluetooth.inf" "$out\package"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
$unsigned=Sha $sys
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& "$ClosureDirectory\signing\bin\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\mt6630-combo-bluetooth.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){& $SignTool verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}}
foreach($p in @($sys,"$out\package\mt6630-combo-bluetooth.inf")){& $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
& llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\client-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
@{schema='mt6630-client-start2014';source_commit=$commit;client_version='2026.11.9.2014';compiled=@('combo_client','combo_controller');reused=$reused;flags=$flags;compiler=(& clang --version);unsigned_sha256=$unsigned;signer_thumbprint=$thumb;capability_admission_changed=$false;registration_deferred_until_physical_owner=$true;real_Code10_root_confirmed=$false;parent_delta_required=$false;provider_rebuilt=$false;audio_rebuilt=$false;hardware_verified=$false;files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\client-signed.zip"
Compress-Archive "$out\obj\*","$out\client-pe.txt" "$out\client-objects.zip"
Get-FileHash "$out\client-signed.zip","$out\client-objects.zip" -Algorithm SHA256
