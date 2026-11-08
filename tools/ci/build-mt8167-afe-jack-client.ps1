# run129のopen error消失だけを修正する1TU client。driver/INF/mediaは変更しない。
param([Parameter(Mandatory=$true)][string]$ClientIncludeDirectory,
 [Parameter(Mandatory=$true)][string]$ArmUserLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$SignTool,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$headers=Get-Content "$ClientIncludeDirectory\header-manifest.json" -Raw|ConvertFrom-Json
foreach($x in $headers){if((Sha (Join-Path $ClientIncludeDirectory $x.path)) -cne $x.sha256){throw 'Fixed WDK81 closure changed'}}
New-Item -ItemType Directory "$out\obj","$out\tmp","$out\package"|Out-Null
$env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$inc=$ClientIncludeDirectory
$args=@('--target=thumbv7-pc-windows-msvc','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-fms-extensions','-std=c11','-D__ldrexd=__a04_afe_jack_wdk81_ldrexd_declaration','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-nostdlibinc','-isystem',"$inc\km\crt",'-isystem',"$inc\um",'-isystem',"$inc\shared",'-w')
$obj="$out\obj\afe-jack-client.obj";$exe="$out\package\mt8167-afe-jack-read.exe"
& clang @args -c "$root\tools\audio\mt8167-afe-jack-read.c" -o $obj
if($LASTEXITCODE){throw 'AFE jack client compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib $obj "$ArmUserLibraryDirectory\kernel32.lib" "$ArmUserLibraryDirectory\setupapi.lib" "$ArmUserLibraryDirectory\cfgmgr32.lib" "/out:$exe"
if($LASTEXITCODE){throw 'AFE jack client link failed'}
$unsigned=Sha $exe;$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
$cert=Get-Item "Cert:\CurrentUser\My\$thumb";if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer required'}
& $SignTool sign /fd SHA256 /sha1 $thumb /s My $exe;if($LASTEXITCODE){throw 'Client sign failed'}
& $SignTool verify /pa /v $exe
if($LASTEXITCODE -or (Get-AuthenticodeSignature $exe).SignerCertificate.Thumbprint -cne $thumb){throw 'Client signature mismatch'}
$pe=& llvm-readobj --file-headers --coff-imports $exe
if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
$pe|Set-Content -Encoding UTF8 ($exe+'.pe.txt')
@{schema='mt8167-afe-jack-open-error-client';source_commit=$commit;compiled_TUs=1;source_sha256=(Sha "$root\tools\audio\mt8167-afe-jack-read.c");unsigned_sha256=$unsigned;signed=$true;signer_thumbprint=$thumb;fixed_args=@('--read');driver_rebuilt=$false;inf_changed=$false;gpio_qualification_relaxed=$false;hardware_verified=$false;files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=$_.Name;sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\afe-jack-client-signed.zip"
Get-FileHash "$out\afe-jack-client-signed.zip" -Algorithm SHA256
'MT8167_AFE_JACK_ACTUAL_OPEN_ERROR_CLIENT_SIGNED_NOT_DEVICE_TESTED'
