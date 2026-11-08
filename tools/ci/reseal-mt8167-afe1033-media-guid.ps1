# INF ClassGuid only. Preserve already signed audio1033 SYS/client bytes.
param([Parameter(Mandatory=$true)][string]$PreviousPackage,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\') -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$m=Get-Content "$PreviousPackage\signed-manifest.json" -Raw|ConvertFrom-Json
if($m.source_commit -cne 'a07dee3fe8b28d0f37bc4fb0f501b0ddcc6b5283'){throw 'Fixed1033 binary source required'}
foreach($x in $m.files){if((Sha (Join-Path $PreviousPackage $x.path)) -cne $x.sha256){throw 'Previous1033 package changed'}}
$sys="$PreviousPackage\audio\mt8167-audio.sys";if((Sha $sys) -cne '1408e60886f6b7ed8a025a288c08b4c6f3282d8864e27e77e4ccb94d43688cf0'){throw 'Signed1033 SYS required'}
$before=Get-Content "$PreviousPackage\audio\mt8167-afe-hp1033.inf" -Raw
$after=Get-Content "$root\drivers\audio\mt8167-afe\mt8167-afe-hp1033.inf" -Raw
if($before.Replace("`r`n","`n").Replace('ClassGuid={4d36e96c-e325-11ce-bfc1-08002be10338}','ClassGuid={4d36e96c-e325-11ce-bfc1-08002be10318}') -cne $after.Replace("`r`n","`n")){throw 'Unexpected INF delta'}
New-Item -ItemType Directory "$out\package\audio","$out\package\clients","$out\tmp"|Out-Null;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
Copy-Item $sys "$out\package\audio"
Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-hp1033.inf" "$out\package\audio"
Copy-Item "$PreviousPackage\clients\mt8167-silent-pcm-consumer.exe" "$out\package\clients"
$sign='C:\mt8167-minimal-service-host\cache\kit81\bin\x86\signtool.exe';$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
& 'C:\mt8167-hp1033\inputs\signing\bin\Inf2Cat.exe' "/driver:$out\package\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'CAT generation failed'}
$cat="$out\package\audio\mt8167-afe-hp1033.cat"
& $sign sign /fd SHA256 /sha1 $thumb /s My $cat;if($LASTEXITCODE){throw 'CAT signing failed'}
foreach($p in @("$out\package\audio\mt8167-audio.sys","$out\package\clients\mt8167-silent-pcm-consumer.exe",$cat)){
 & $sign verify /pa /v $p;if($LASTEXITCODE -or (Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -cne $thumb){throw 'Signature identity failed'}
}
foreach($p in @("$out\package\audio\mt8167-audio.sys","$out\package\audio\mt8167-afe-hp1033.inf")){& $sign verify /pa /v /c $cat $p;if($LASTEXITCODE){throw 'CAT member failed'}}
if((Sha "$out\package\audio\mt8167-audio.sys") -cne (Sha $sys) -or (Sha "$out\package\clients\mt8167-silent-pcm-consumer.exe") -cne (Sha "$PreviousPackage\clients\mt8167-silent-pcm-consumer.exe")){throw 'Binary changed'}
@{schema='mt8167-audio1033-media-guid-corrected';source_commit=$commit;binary_source_commit=$m.source_commit;version='2026.11.8.1033';change='ClassGuid only 10338 to MEDIA10318';SYS_rebuilt=$false;client_rebuilt=$false;SYS_resigned=$false;CAT_regenerated_signed=$true;signed=$true;signer_thumbprint=$thumb;hardware_verified=$false;install1306_causality_proved=$false;hp_abi_version=2;hp_abi_build=1107;thermal_qi_build=1104;files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Sha $_.FullName)}})}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive "$out\package\*" "$out\audio1033-media-guid-signed.zip"
Get-FileHash "$out\audio1033-media-guid-signed.zip" -Algorithm SHA256
'MT8167_AUDIO1033_INF_ONLY_RESEALED_NOT_DEVICE_TESTED'
