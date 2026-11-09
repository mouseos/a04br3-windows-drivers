param([Parameter(Mandatory=$true)][string]$ExistingPackage,[Parameter(Mandatory=$true)][string]$Inf2Cat,
 [Parameter(Mandatory=$true)][string]$SignTool,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path $out){throw 'New output only'}
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or @(& git -C $root status --porcelain).Count){throw 'Clean source and checkout-local output required'}
$sha={param($p)(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$expected='1408e60886f6b7ed8a025a288c08b4c6f3282d8864e27e77e4ccb94d43688cf0'
if((& $sha (Join-Path $ExistingPackage 'mt8167-audio.sys')) -cne $expected){throw 'Existing exact signed1033 SYS required'}
$inf=Join-Path $root 'drivers\audio\mt8167-afe\mt8167-afe-hp1033.inf'
$text=Get-Content $inf -Raw
if($text -notmatch '(?im)^ClassGuid=\{4d36e96c-e325-11ce-bfc1-08002be10318\}\s*$' -or $text -notmatch '(?m)^DriverVer=11/08/2026,2026\.11\.8\.1033\s*$'){throw 'Exact MEDIA class and1033 version required'}
New-Item -ItemType Directory "$out\audio"|Out-Null
Copy-Item (Join-Path $ExistingPackage 'mt8167-audio.sys') "$out\audio\mt8167-audio.sys"
Copy-Item $inf "$out\audio\mt8167-afe-hp1033.inf"
& $Inf2Cat "/driver:$out\audio" /os:6_3_ARM /uselocaltime;if($LASTEXITCODE){throw 'Corrected catalog generation failed'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
& $SignTool sign /fd SHA256 /sha1 $thumb /s My "$out\audio\mt8167-afe-hp1033.cat";if($LASTEXITCODE){throw 'CAT sign failed'}
foreach($leaf in @('mt8167-afe-hp1033.cat','mt8167-audio.sys')){if((Get-AuthenticodeSignature "$out\audio\$leaf").SignerCertificate.Thumbprint -cne $thumb){throw 'Retained identity mismatch'}}
foreach($leaf in @('mt8167-audio.sys','mt8167-afe-hp1033.inf')){& $SignTool verify /pa /c "$out\audio\mt8167-afe-hp1033.cat" "$out\audio\$leaf";if($LASTEXITCODE){throw 'Corrected CAT membership/trust failed'}}
if((& $sha "$out\audio\mt8167-audio.sys") -cne $expected){throw 'SYS changed during repack'}
$m=[ordered]@{schema='afe1033-media-class-catalog-only-fix-v1';source_commit=(& git -C $root rev-parse HEAD).Trim();binary_source_commit='a07dee3fe8b28d0f37bc4fb0f501b0ddcc6b5283';object_source_commit='950f0a3fc61c677b6191921f35fb259a8e4841f4';driver_version='2026.11.8.1033';ClassGuid='{4d36e96c-e325-11ce-bfc1-08002be10318}';source_dirty=$false;SYS_byte_retained=$true;SYS_recompiled=$false;client_recompiled=$false;parent_recompiled=$false;catalog_regenerated=$true;runtime_guard_relaxed=$false;signer_thumbprint=$thumb;signature_status='Valid';hardware_verified=$false;files=@(Get-ChildItem "$out\audio" -File|ForEach-Object{@{path='audio/'+$_.Name;sha256=(& $sha $_.FullName);bytes=$_.Length}})}
$m|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
Compress-Archive "$out\audio","$out\signed-manifest.json" "$out\afe1033-media-class-fixed.zip"
'AFE1033_INF_CAT_FIXED_SYS_BYTE_RETAINED_NOT_DEVICE_TESTED'
