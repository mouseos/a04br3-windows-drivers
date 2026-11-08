# MT8167 thermal child1TUだけ増分。親/UEFI/AFE非変更。
param([Parameter(Mandatory=$true)][string]$ClosureDirectory,
 [Parameter(Mandatory=$true)][string]$KitIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$DependencyDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$UserIncludeDirectory='C:\mt8167-minimal-service-host\cache\kit81\Include\um',
 [string]$UserLibraryDirectory='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$ClosureDirectory\cache\thermal-cache.json" -Raw|ConvertFrom-Json
if($m.source_commit -cne '1d248a71a95360966ff867e4b063504ac764e074' -or $m.objects.Count -ne 3){throw 'Fixed 3-object client cache required'}
foreach($f in $m.files.PSObject.Properties){if((Sha (Join-Path $ClosureDirectory $f.Name)) -cne $f.Value){throw 'Closure bytes changed'}}
$objects=@();$reused=@()
foreach($x in $m.objects){
 foreach($f in $x.verified_source_closure.PSObject.Properties){if((Sha (Join-Path $root $f.Name)) -cne $f.Value){throw 'Reused object source closure changed'}}
 $p=Join-Path "$ClosureDirectory\cache" ($x.name+'.obj');if((Sha $p) -cne $x.sha256.ToLowerInvariant()){throw 'Reused object changed'};$objects+=$p;$reused+=$x
}
New-Item -ItemType Directory "$out\obj","$out\package","$out\tmp"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$wdk="$DependencyDirectory\wdk";$km="$wdk\Lib\winv6.3\km\arm";$wdf="$wdk\Lib\wdf\kmdf\arm\1.13"
$flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fno-stack-protector','-ffunction-sections','-fdata-sections','-w','-D__ldrexd=__combo_wdk81_ldrexd','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-isystem',"$KitIncludeDirectory\km",'-isystem',"$KitIncludeDirectory\shared",'-isystem',"$KitIncludeDirectory\um",'-isystem',"$wdk\Include\wdf\kmdf\1.13")
$obj="$out\obj\wdf_native_thermal.obj"
& clang @flags -c "$root\drivers\power\mt8167-thermal\wdf_native_thermal.c" -o $obj;if($LASTEXITCODE){throw 'Client compile failed'}
$objects=@($obj)+$objects;$sys="$out\package\mt8167-thermal.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:noref @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$sys"
if($LASTEXITCODE){throw 'Client link failed'}
Copy-Item "$root\drivers\power\mt8167-thermal\mt8167-thermal-native.inf" "$out\package\mt8167-thermal.inf"
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
New-Item -ItemType Directory "$out\client"|Out-Null
$umflags=@('-D__ldrexd=__mt8167_thermal_status_wdk81_ldrexd','-nostdlibinc','--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fms-extensions','-fno-stack-protector','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-isystem',"$UserIncludeDirectory",'-isystem',"$KitIncludeDirectory\shared",'-isystem',"$KitIncludeDirectory\km\crt")
& clang @umflags -c "$root\tools\thermal\mt8167-thermal-ioctl-status.c" -o "$out\obj\ioctl_status_client.obj"
if($LASTEXITCODE){throw 'Fixed status client compile failed'}
$client="$out\client\mt8167-thermal-ioctl-status.exe"
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\obj\ioctl_status_client.obj" "$UserLibraryDirectory\kernel32.lib" "$UserLibraryDirectory\setupapi.lib" "/out:$client"
if($LASTEXITCODE){throw 'Fixed status client link failed'}
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $client
if($LASTEXITCODE){throw 'Status client signing failed'}
& $SignTool verify /pa /v $client
if($LASTEXITCODE -or (Get-AuthenticodeSignature $client).SignerCertificate.Thumbprint -cne $thumb){throw 'Status client signer mismatch'}
$unsigned=Sha $sys
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& "$DependencyDirectory\signing\bin\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\mt8167-thermal.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){& $SignTool verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signer mismatch'}}
foreach($p in @($sys,"$out\package\mt8167-thermal.inf")){& $SignTool verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT membership failed'}}
& llvm-readobj --file-headers --coff-imports $sys|Set-Content -Encoding UTF8 "$out\client-pe.txt";if($LASTEXITCODE){throw 'PE metadata failed'}
@{schema='mt8167-thermal-first-read';source_commit=$commit;driver_version='0.0.1.10';compiled=@('wdf_native_thermal');reused=$reused;flags=$flags;compiler=(& clang --version);unsigned_sha256=$unsigned;signer_thumbprint=$thumb;parent_delta_required=$false;parent_rebuilt=$false;audio_rebuilt=$false;measurement_on_first_valid_read=$true;parent_one_shot_retained=$true;max_age_ms=1000;hardware_verified=$false;files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 14|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
@{source_commit=$commit;client_sha256=(Sha $client);fixed_ioctl='0x226814';input_bytes=0;output_bytes=64;invocation='--status';fixed_output='thermal-ioctl-status-v1.bin';hardware_read_triggered=$false;temperature_success_claimed=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$out\client\manifest.json"
Compress-Archive "$out\client\*" "$out\ioctl-status-client.zip"
Compress-Archive "$out\package\*" "$out\client-signed.zip"
Compress-Archive "$out\obj\*","$out\client-pe.txt" "$out\client-objects.zip"
Get-FileHash "$out\client-signed.zip","$out\client-objects.zip" -Algorithm SHA256
