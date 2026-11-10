param(
 [string]$PinsHeader,[string]$Inputs,[string]$ExpectedInputClosureSha256,
 [Parameter(Mandatory=$true)][string]$BaselineManifest,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$KitHeaderRoot='C:\mt8167-minimal-service-host\cache\kit81\Include',
 [string]$LibraryRoot='C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm',
 [string]$SignTool='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe'
)
# 既存入力固定のSCM componentのみ。native/profile/mediaを生成しない。
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$root=[IO.Path]::GetFullPath("$PSScriptRoot\..\..");$out=[IO.Path]::GetFullPath($OutputDirectory)
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count -or !$out.StartsWith($root+'\build\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Clean source and fresh build output required'}
if((Sha $BaselineManifest) -cne '6b8bc0005b22d2ecd8b4967fc46a59eff28e849d598cf9a1dea282f621d5c91d'){throw 'Exact signed719 baseline manifest required'}
$bound=!!$Inputs;$binding=$null;$profile=$null
if($bound){
 if($PinsHeader -or !$ExpectedInputClosureSha256 -or (Sha "$Inputs\input-closure.json") -cne $ExpectedInputClosureSha256){throw 'Pinned final input closure required'}
 $binding=Get-Content "$Inputs\input-closure.json" -Raw|ConvertFrom-Json
 foreach($f in $binding.files.PSObject.Properties){$q=[IO.Path]::GetFullPath((Join-Path $Inputs $f.Name));if(!$q.StartsWith([IO.Path]::GetFullPath($Inputs).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or (Sha $q) -cne $f.Value){throw 'Final input leaf drift'}}
 $PinsHeader="$Inputs\usb1121-service-pins.h";$profile=Get-Content "$Inputs\payload\profile.json" -Raw|ConvertFrom-Json
 if($profile.schema -cne 'MT8167_USB1121_REBOUND_SERVICE_V2' -or $profile.fresh_sha256 -cne $binding.fresh_sha256 -or $profile.target_gpu_version -cne '0.0.3.69' -or $profile.integration_phase -cne 'after-gpu369-before-final-flush' -or (Sha "$Inputs\payload\profile.json") -cne $binding.profile_sha256){throw 'Final rebound profile required'}
 if((Sha "$Inputs\payload\run-usb1121-service.ps1") -cne (Sha "$root\tools\usb\run-usb1121-service.ps1")){throw 'Published launcher required'}
 $header=Get-Content $PinsHeader -Raw
 foreach($pair in @(@('launcher',(Sha "$Inputs\payload\run-usb1121-service.ps1")),@('profile',(Sha "$Inputs\payload\profile.json")))){
  $parts=@();for($i=0;$i -lt 64;$i+=2){$parts+=('0x'+$pair[1].Substring($i,2))}
  if(!$header.Contains(('static const BYTE '+$pair[0]+'_hash[32]={'+($parts -join ',')+'};'))){throw 'Generated exact header mismatch'}
 }
}else{if(!$PinsHeader -or (Sha $PinsHeader) -cne '4263a1e09942d3dbb15a1a1a4e973a024b3ad35790172bbdb7b879b3b0cc5f7c'){throw 'Exact legacy input header required'}}
if((Sha $SignTool) -cne '854c356a9d9977fb2ed22ce04b531ab6f4417da06a4dba6e3d3d628b9cfb988c'){throw 'Existing approved signtool required'}
$old=Get-Content $BaselineManifest -Raw|ConvertFrom-Json
$libs=@();$libpins=@();foreach($n in @('kernel32','advapi32','bcrypt','wintrust','crypt32')){
 $p=Join-Path $LibraryRoot ($n+'.lib');$want=@($old.import_libraries|Where-Object name -CEQ $n)
 if($want.Count -ne 1 -or (Sha $p) -cne $want[0].sha256){throw ('Existing library mismatch '+$n)}
 $libs+=$p;$libpins+=@{name=$n;sha256=(Sha $p)}
}
$runtime='5c39277db113faa98cd3a0f26c1072a7ce3d76c2'
$closure=[ordered]@{}
foreach($n in @('tools/usb/mt8167-usb1121-install-service.c','tools/usb/usb1121-service-uart.h','tools/usb/usb1121-system-volume-flush.h','drivers/system/mt8167-toprgu/uart_screen.h','tools/usb/powershell-system-image-pin.json','tools/usb/usb1121-uart198-contract.json')){
 & git -C $root diff --quiet $runtime HEAD -- $n;if($LASTEXITCODE){throw 'Runtime source differs from fixed5c392'}
 $closure[$n]=Sha (Join-Path $root $n)
}
New-Item -ItemType Directory "$out\payload","$out\tmp"|Out-Null
if($bound){
 Copy-Item "$Inputs\payload\*" "$out\payload" -Recurse
 $h=Get-AuthenticodeSignature "$out\payload\helper\MT8167UsbAbi3Pnp.dll"
 if($h.Status -ne 'Valid' -or $h.SignerCertificate.Thumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Existing signed helper trust'}
 & $SignTool verify /pa /all "$out\payload\helper\MT8167UsbAbi3Pnp.dll";if($LASTEXITCODE){throw 'Helper verify'}
 $plan=Get-Content "$out\payload\bundle\plan.json" -Raw|ConvertFrom-Json
 foreach($e in @($plan.candidate.parent,$plan.candidate.hcd,$plan.rollback.parent,$plan.rollback.legacy)){
  $cat=Join-Path "$out\payload\bundle" $e.names.cat;$sig=Get-AuthenticodeSignature $cat
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Existing catalog trust'}
  foreach($n in @($e.names.inf,$e.names.sys)){& $SignTool verify /pa /c $cat (Join-Path "$out\payload\bundle" $n);if($LASTEXITCODE){throw 'Existing CAT member verify'}}
 }
}
Copy-Item $PinsHeader "$out\usb1121-service-pins.h"
$receipt=[ordered]@{schema='MT8167_USB1121_PS_IMAGE_COMPONENT_V1';status='started';source_commit=$commit;runtime_source_commit=$runtime;source_closure=$closure;pins_header_sha256=(Sha $PinsHeader);baseline_manifest_sha256=(Sha $BaselineManifest);profile_sha256=$old.profile_sha256;fresh_sha256=$old.fresh_sha256;service=$old.service;service_compile_TUs=0;driver_rebuilds=0;helper_rebuilds=0;SDK_setup=$false;native_preparation_invocations=0;profile_generated=$false;media_generated=$false;deployment_scope='component-only; exact legacy fresh198/GPU367 pins retained; not a fresh206/GPU369 deployment package';hardware_applied=$false;power_verified=$false}
if($bound){
 $receipt.profile_sha256=$binding.profile_sha256;$receipt.fresh_sha256=$binding.fresh_sha256
 $receipt.launcher_source_sha256=Sha "$root\tools\usb\run-usb1121-service.ps1";$receipt.input_closure_sha256=$ExpectedInputClosureSha256;$receipt.final_gpu_manifest_sha256=$profile.final_gpu_manifest_sha256
 $receipt.final_policy_sha256=$profile.final_policy_sha256;$receipt.native_receipt_sha256=$profile.native_receipt_sha256
 $receipt.integration_phase=$profile.integration_phase;$receipt.deployment_scope='final-pinned payload component; main combined GPU phase only; no native hive or automatic activation'
}
function Save {$receipt|ConvertTo-Json -Depth 25|Set-Content -Encoding UTF8 "$out\component-manifest.json"}
Save
$cc=(Get-Command clang).Source;$ld=(Get-Command lld-link).Source;$name=$old.service;$exe="$out\payload\$name.exe"
$ccargs=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fms-extensions','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',"-I$KitHeaderRoot\um","-I$KitHeaderRoot\shared","-I$out",'-c',"$root\tools\usb\mt8167-usb1121-install-service.c",'-o',"$out\$name.obj")
$ldargs=@('/subsystem:console,6.3','/machine:arm','/entry:mainCRTStartup','/nodefaultlib','/dynamicbase','/nxcompat',"$out\$name.obj")+$libs+@("/out:$exe")
$saveTmp=$env:TMP;$saveTemp=$env:TEMP
try{
 $env:TMP="$out\tmp";$env:TEMP=$env:TMP
 & $cc @ccargs;if($LASTEXITCODE){throw 'Service1TU compile failed'};$receipt.service_compile_TUs=1
 & $ld @ldargs;if($LASTEXITCODE){throw 'Service link failed'}
 $receipt.unsigned_exe_sha256=Sha $exe
 $thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
 & $SignTool sign /fd SHA256 /s My /sha1 $thumb $exe;if($LASTEXITCODE){throw 'Service sign failed'}
 & $SignTool verify /pa /all $exe;if($LASTEXITCODE){throw 'Service verify failed'}
 $sig=Get-AuthenticodeSignature $exe;if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw 'Service Authenticode mismatch'}
 $receipt.signature_status=[string]$sig.Status;$receipt.signer_thumbprint=$sig.SignerCertificate.Thumbprint
 $receipt.signed_exe_sha256=Sha $exe;$receipt.object_sha256=Sha "$out\$name.obj"
 $receipt.compile_args=$ccargs;$receipt.link_args=$ldargs;$receipt.import_libraries=$libpins
 $receipt.tools=@(@{path=$cc;sha256=(Sha $cc)},@{path=$ld;sha256=(Sha $ld)},@{path=$SignTool;sha256=(Sha $SignTool)})
 $receipt.ps_exact_image=Get-Content "$root\tools\usb\powershell-system-image-pin.json" -Raw|ConvertFrom-Json
 $receipt.status=if($bound){'signed-final-pinned-payload-not-hardware'}else{'signed-component-verified-not-deployment-or-power'}
 $receipt.files=[ordered]@{};Get-ChildItem "$out\payload" -File -Recurse|ForEach-Object{$receipt.files[$_.FullName.Substring($out.Length+1).Replace('\','/')]=Sha $_.FullName};Save
 Compress-Archive "$out\payload","$out\$name.obj","$out\usb1121-service-pins.h","$out\component-manifest.json" "$out\usb1121-ps-image-component.zip"
 'USB1121_PS_IMAGE_COMPONENT_SIGNED_NOT_POWER_SUCCESS'
}catch{$receipt.status='failed';$receipt.error=$_.Exception.ToString();Save;throw}finally{$env:TMP=$saveTmp;$env:TEMP=$saveTemp}
