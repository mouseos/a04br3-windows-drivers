# Installed c6/1.9 source delta only; no device/media/parent work.
param([Parameter(Mandatory=$true)][string]$KitRoot,[Parameter(Mandatory=$true)][string]$UmInclude,
 [Parameter(Mandatory=$true)][string]$ReuseDirectory,[Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out) -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source and new output required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$commit=(& git -C $root rev-parse HEAD).Trim();$input=Get-Content "$PSScriptRoot\msdc19-runtime-flags.json" -Raw|ConvertFrom-Json
if((Sha "$BaselineDirectory\mt8167-msdc.sys") -cne $input.baseline_SYS){throw 'Actual fresh144/1.9 baseline mismatch'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
foreach($n in @('mt8167-msdc.sys','mt8167-msdc.inf')){
 & $SignTool verify /pa /v /c "$BaselineDirectory\mt8167-msdc.cat" "$BaselineDirectory\$n"
 if($LASTEXITCODE){throw 'Original1.9 CAT membership failed'}
}
if((Get-AuthenticodeSignature "$BaselineDirectory\mt8167-msdc.sys").SignerCertificate.Thumbprint -cne $thumb){throw 'Original1.9 signer mismatch'}
New-Item -ItemType Directory "$out\obj","$out\package","$out\tmp"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
$reuse=Get-Content "$ReuseDirectory\reuse-manifest.json" -Raw|ConvertFrom-Json
if(((& clang --version)[0]) -cne $reuse.compiler_first_line){throw 'Compiler cache mismatch'}
$objects=@();$reused=@()
foreach($x in $reuse.objects){
 if($x.name -notin @('pio','cdb','scsi_capacity','scsi','gpt_guard')){throw 'Unexpected object'}
 foreach($dep in $x.dependencies.PSObject.Properties){
  if((Sha (Join-Path $root $dep.Name)) -cne $dep.Value){throw 'Reused source/include hash drift'}
  $old=(& git -C $root rev-parse "$($x.source):$($dep.Name)")
  $new=(& git -C $root rev-parse "HEAD:$($dep.Name)")
  if($LASTEXITCODE -or $old -cne $new){throw 'Reused source/include blob drift'}
 }
 $obj="$ReuseDirectory\$($x.name).obj";if((Sha $obj) -cne $x.sha256){throw 'Reused object mismatch'}
 $objects+=$obj;$reused+=$x
}
if($objects.Count -ne 5){throw 'Exactly5 unchanged portable objects required'}
$inc="$KitRoot\Include";$lib="$KitRoot\Lib\winv6.3\km\arm";$dir="$root\drivers\storage\mt8167-msdc"
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c "$dir\dma.c" -o "$out\obj\dma.obj"
if($LASTEXITCODE){throw 'Actual1.9 DMA core compile failed'}
$objects+="$out\obj\dma.obj"
$flags=@($input.defines)+@('-DA04_MSDC_REQUEST_QPC_REUSE=1','-DA04_MSDC_REQUEST_COPY_OPT=1')
$common=@('--target=thumbv7-pc-windows-msvc','-w','-fms-extensions','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$UmInclude")
& clang @common @flags -c "$dir\miniport.c" -o "$out\obj\miniport.obj"
if($LASTEXITCODE){throw 'Performance miniport compile failed'}
$objects+="$out\obj\miniport.obj"
# Exact six imports of the actual1.9 SYS, declarations remain WDK8.1 types.
@('LIBRARY storport.sys','EXPORTS',' StorPortExtendedFunction',' StorPortGetDeviceBase',' StorPortGetPhysicalAddress',' StorPortGetUncachedExtension',' StorPortInitialize',' StorPortNotification')|Set-Content -Encoding ASCII "$out\storport.def"
& lld-link /lib /machine:arm "/def:$out\storport.def" "/out:$out\storport.lib"
if($LASTEXITCODE){throw 'Storport import library failed'}
$sys="$out\package\mt8167-msdc.sys"
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects "$out\storport.lib" "$lib\ntoskrnl.lib" "$lib\hal.lib" "/out:$sys" "/map:$out\msdc.map"
if($LASTEXITCODE){throw 'MSDC performance link failed'}
# Compile switch-off against the same WDK to retain the comparison profile.
& clang @common @($input.defines) -c "$dir\miniport.c" -o "$out\obj\miniport-disabled.obj"
if($LASTEXITCODE){throw 'Disabled comparison compile failed'}
Copy-Item "$dir\mt8167-msdc-perf110.inf" "$out\package\mt8167-msdc.inf"
$unsigned=Sha $sys;$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey){throw 'Existing signer required'}
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $sys;if($LASTEXITCODE){throw 'SYS sign failed'}
& $Inf2Cat "/driver:$out\package" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\mt8167-msdc.cat"
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($p in @($sys,$cat)){
 & $SignTool verify /pa /v $p
 if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature/trust mismatch'}
}
foreach($n in @('mt8167-msdc.sys','mt8167-msdc.inf')){& $SignTool verify /pa /v /c $cat "$out\package\$n";if($LASTEXITCODE){throw 'New CAT membership failed'}}
$pe=& llvm-readobj --file-headers --coff-imports $sys
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 "$out\pe.txt"
@{schema='msdc-request-perf110';source_commit=$commit;baseline_runtime=$input.runtime_source;baseline_actual_SYS=$input.baseline_SYS;
 driver_version='0.0.1.10';flags=$flags;compiler=(clang --version)-join "`n";compiled_TUs=2;reused_objects=$reused;
 comparison_profile_compiled=$true;unsigned_SYS_sha256=$unsigned;signer=$thumb;signed=$true;
 MMC_clock_voltage_tuning_changed=$false;DMA_command_stop_retry_changed=$false;cache_flush_changed=$false;parent_USB_GPU_changed=$false;
 hardware_verified=$false;speed_measured=$false;deployment_performed=$false;
 files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})
}|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\msdc-perf110-signed.zip"
Compress-Archive "$out\obj\*","$out\*.map","$out\pe.txt" "$out\msdc-perf110-evidence.zip"
Get-FileHash "$out\msdc-perf110-signed.zip","$out\msdc-perf110-evidence.zip" -Algorithm SHA256|ForEach-Object{Write-Output ($_.Path+' '+$_.Hash)}
'MSDC_PERF110_SIGNED_NOT_DEVICE_TESTED'
