param(
 [Parameter(Mandatory=$true)][string]$PinsHeader,
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
if((Sha $PinsHeader) -cne '4263a1e09942d3dbb15a1a1a4e973a024b3ad35790172bbdb7b879b3b0cc5f7c'){throw 'Exact legacy input header required'}
if((Sha $SignTool) -cne '854c356a9d9977fb2ed22ce04b531ab6f4417da06a4dba6e3d3d628b9cfb988c'){throw 'Existing approved signtool required'}
$old=Get-Content $BaselineManifest -Raw|ConvertFrom-Json
$libs=@();$libpins=@();foreach($n in @('kernel32','advapi32','bcrypt','wintrust','crypt32')){
 $p=Join-Path $LibraryRoot ($n+'.lib');$want=@($old.import_libraries|Where-Object name -CEQ $n)
 if($want.Count -ne 1 -or (Sha $p) -cne $want[0].sha256){throw ('Existing library mismatch '+$n)}
 $libs+=$p;$libpins+=@{name=$n;sha256=(Sha $p)}
}
$runtime='5c39277db113faa98cd3a0f26c1072a7ce3d76c2'
$closure=[ordered]@{}
foreach($n in @('tools/usb/mt8167-usb1121-install-service.c','tools/usb/usb1121-service-uart.h','tools/usb/usb1121-system-volume-flush.h','drivers/system/mt8167-toprgu/uart_screen.h','tools/usb/powershell-system-image-pin.json','tools/usb/usb1121-uart198-contract.json','tools/usb/run-usb1121-service.ps1')){
 & git -C $root diff --quiet $runtime HEAD -- $n;if($LASTEXITCODE){throw 'Runtime source differs from fixed5c392'}
 $closure[$n]=Sha (Join-Path $root $n)
}
New-Item -ItemType Directory "$out\payload","$out\tmp"|Out-Null
Copy-Item $PinsHeader "$out\usb1121-service-pins.h"
$receipt=[ordered]@{schema='MT8167_USB1121_PS_IMAGE_COMPONENT_V1';status='started';source_commit=$commit;runtime_source_commit=$runtime;source_closure=$closure;pins_header_sha256=(Sha $PinsHeader);baseline_manifest_sha256=(Sha $BaselineManifest);profile_sha256=$old.profile_sha256;fresh_sha256=$old.fresh_sha256;service=$old.service;service_compile_TUs=0;driver_rebuilds=0;helper_rebuilds=0;SDK_setup=$false;native_preparation_invocations=0;profile_generated=$false;media_generated=$false;deployment_scope='component-only; exact legacy fresh198/GPU367 pins retained; not a fresh206/GPU369 deployment package';hardware_applied=$false;power_verified=$false}
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
 $receipt.status='signed-component-verified-not-deployment-or-power';Save
 Compress-Archive "$out\payload","$out\$name.obj","$out\usb1121-service-pins.h","$out\component-manifest.json" "$out\usb1121-ps-image-component.zip"
 'USB1121_PS_IMAGE_COMPONENT_SIGNED_NOT_POWER_SUCCESS'
}catch{$receipt.status='failed';$receipt.error=$_.Exception.ToString();Save;throw}finally{$env:TMP=$saveTmp;$env:TEMP=$saveTemp}
