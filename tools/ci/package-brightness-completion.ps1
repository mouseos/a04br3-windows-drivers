# Repackage an existing signed client; no compilation, signing or deployment.
param([Parameter(Mandatory=$true)][string]$InputDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$source='8a9354a0f49788b45f72a7b512ce73e36da8e3bc'
$signer='BCBF6C456C6F0DD0818434763B189488FBF13570'
$inputPath=[IO.Path]::GetFullPath($InputDirectory)
$outputPath=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path $outputPath){throw 'Fresh output directory required'}
$metadata=Get-Content -Raw "$inputPath\manifest.json"|ConvertFrom-Json
$image="$inputPath\mt8167-brightness-native.exe"
$hash=(Get-FileHash $image -Algorithm SHA256).Hash.ToLowerInvariant()
if($metadata.commit -cne $source -or $metadata.dirty -ne $false -or $metadata.signed -ne $true -or
   $metadata.machine -cne 'ARMNT' -or $metadata.arguments -cne 'none' -or
   $metadata.wmi_completion -cne 'GetCallStatus-before-GetResultObject' -or
   $metadata.log_path -cne 'C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log' -or
   $metadata.stdout_tee -ne $true -or $metadata.stdout_max_bytes -ne 16384 -or
   $metadata.legacy_root_log_modified -ne $false -or $metadata.sys_or_cat_modified -ne $false -or
   $metadata.log_generation -cne 'Engine-state-archive' -or $metadata.exe_sha256.ToLowerInvariant() -cne $hash){
 throw 'Fixed completion client build contract mismatch'
}
$signature=Get-AuthenticodeSignature $image
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -cne $signer){throw 'Existing client signature rejected'}
New-Item -ItemType Directory $outputPath|Out-Null
Copy-Item $image "$outputPath\mt8167-brightness-native.exe"
Copy-Item "$inputPath\manifest.json" "$outputPath\manifest.json"
Export-Certificate -Cert $signature.SignerCertificate -FilePath "$outputPath\mt8167-development.cer"|Out-Null
$files=@('mt8167-brightness-native.exe','manifest.json','mt8167-development.cer')|ForEach-Object {
 @{path=$_;sha256=(Get-FileHash "$outputPath\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}
}
@{source_commit=$source;signed=$true;signing_thumbprint=$signer;signature_status='Valid';
 binary_recompiled=$false;binary_resigned=$false;device_verified=$false;files=@($files)}|
 ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$outputPath\signed-manifest.json"
