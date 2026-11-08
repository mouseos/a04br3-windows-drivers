# Reuse the pinned compiled parent/client COFF; only client link and signing.
param([Parameter(Mandatory=$true)][string]$CompileDirectory,
 [Parameter(Mandatory=$true)][string]$ToolDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$SigntoolPath,
 [Parameter(Mandatory=$true)][string]$ArmUserLibraryDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
$compiled=[IO.Path]::GetFullPath($CompileDirectory)
foreach($p in @($out,$compiled)){if(!$p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Checkout-local dedicated paths required'}}
if(Test-Path $out){throw 'New output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$binarySource='204335687c6267310947007c61affba9cf8589ca'
$m=Get-Content "$compiled\manifest.json" -Raw|ConvertFrom-Json
if($m.source_commit -ne $binarySource -or $m.reused_helper_objects -ne 13 -or $m.readonly_flag -ne 'A04_SHARED_ACCDET_READ=1'){throw 'Fixed ACCDET compile manifest required'}
foreach($f in $m.files){if((Get-FileHash "$compiled\$($f.path)" -Algorithm SHA256).Hash -ne $f.sha256){throw 'Compiled artifact mismatch'}}
if((Get-FileHash "$compiled\mt8167-accdet-readonly-compile-only.sys" -Algorithm SHA256).Hash -ne '191E6A1353C76181271B23A92C2D4DEB7E715F4CBDAA181238A3354D59347757'){throw 'Fixed enabled SYS required'}
$allowed=@('drivers/input/mt8167-accdet/README.md','drivers/input/mt8167-accdet/mt8167-shared-accdet1102.inf')
$changes=@(& git -C $root diff --name-only $binarySource HEAD -- drivers tools/input)
if($LASTEXITCODE -or @($changes|Where-Object{$_ -notin $allowed}).Count){throw 'Runtime source drift forbids compiled reuse'}
$thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';$cert=Get-Item "Cert:\CurrentUser\My\$thumb"
if(!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)){throw 'Existing signer key unavailable'}
New-Item -ItemType Directory "$out\package\parent","$out\package\client"|Out-Null
$parent="$out\package\parent";$client="$out\package\client"
Copy-Item "$compiled\mt8167-accdet-readonly-compile-only.sys" "$parent\mt8167-shared.sys"
Copy-Item "$root\drivers\input\mt8167-accdet\mt8167-shared-accdet1102.inf" $parent
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$compiled\mt8167-accdet-read.obj" "$ArmUserLibraryDirectory\kernel32.lib" "$ArmUserLibraryDirectory\setupapi.lib" "/out:$client\mt8167-accdet-read.exe"
if($LASTEXITCODE){throw 'Read-only client link failed'}
foreach($p in @("$parent\mt8167-shared.sys","$client\mt8167-accdet-read.exe")){
 & $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My $p;if($LASTEXITCODE){throw 'Signing failed'}
 & $SigntoolPath verify /pa /v $p;if($LASTEXITCODE){throw 'Signature verification failed'}
 if((Get-AuthenticodeSignature $p).SignerCertificate.Thumbprint -ne $thumb){throw 'Wrong signer'}
}
& "$ToolDirectory\bin\Inf2Cat.exe" "/driver:$parent" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE){throw 'ARM8.1 catalog generation failed'}
& $SigntoolPath sign /fd SHA256 /sha1 $thumb /s My "$parent\mt8167-shared-accdet.cat"
if($LASTEXITCODE){throw 'Catalog signing failed'}
& $SigntoolPath verify /pa /v "$parent\mt8167-shared-accdet.cat"
if($LASTEXITCODE -or (Get-AuthenticodeSignature "$parent\mt8167-shared-accdet.cat").SignerCertificate.Thumbprint -ne $thumb){throw 'Catalog signature verification failed'}
foreach($name in @('mt8167-shared.sys','mt8167-shared-accdet1102.inf')){
 & $SigntoolPath verify /pa /v /c "$parent\mt8167-shared-accdet.cat" "$parent\$name"
 if($LASTEXITCODE){throw 'Catalog membership verification failed'}
}
foreach($p in @("$parent\mt8167-shared.sys","$client\mt8167-accdet-read.exe")){
 $pe=& llvm-readobj --file-headers --coff-imports $p
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARM32 metadata required'}
 $pe|Set-Content -Encoding UTF8 ($p+'.pe.txt')
}
Export-Certificate -Cert $cert -FilePath "$out\package\development.cer"|Out-Null
Copy-Item "$compiled\manifest.json" "$out\package\compiled-manifest.json"
$files=@(Get-ChildItem "$out\package" -Recurse -File|ForEach-Object{@{path=$_.FullName.Substring(("$out\package\").Length).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
@{source_commit=$commit;binary_source_commit=$binarySource;baseline_parent_source='1d248a71a95360966ff867e4b063504ac764e074';baseline_parent_signed_sys_sha256='398fd007079ab4710404d67a60dc7db46b84dd149ce3ae69e3bbc730264e823c';driver_version='2026.11.8.1102';baseline_driver_version='2026.11.8.1101';signer_thumbprint=$thumb;signed=$true;source_dirty=$false;parent_recompiled=$false;client_linked=$true;readonly_flag='A04_SHARED_ACCDET_READ=1';baseline_parent_flags=@($m.parent_flags);flag_delta=@{A04_SHARED_ACCDET_READ='1'};eight_range_source_unchanged=$true;hardware_verified=$false;gpio_write_added=$false;audio_endpoint_added=$false;wifi_or_bt_change_added=$false;files=$files}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\package\signed-manifest.json"
Compress-Archive -Path "$out\package\*" -DestinationPath "$out\mt8167-accdet1102-signed.zip"
'MT8167_ACCDET1102_SIGNED_COMPLETE'
