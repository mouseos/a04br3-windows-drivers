# Signature identity verification only; no import/export, install or device call.
param([Parameter(Mandatory=$true)][string]$PathsJson,
 [Parameter(Mandatory=$true)][string]$ExpectedThumbprint,
 [Parameter(Mandatory=$true)][string]$ReportJson)
$ErrorActionPreference='Stop'
$thumb=$ExpectedThumbprint.ToUpperInvariant()
if($thumb -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Existing BCBF identity required'}
$rows=@()
foreach($file in @(Get-Content -LiteralPath $PathsJson -Raw|ConvertFrom-Json)){
 $sig=Get-AuthenticodeSignature -LiteralPath $file
 if($sig.Status -ne 'Valid' -or !$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw "Signature identity/trust failed: $file"}
 $rows+=@{path=$file;status=[string]$sig.Status;signer_thumbprint=$sig.SignerCertificate.Thumbprint;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()}
}
if(!$rows.Count){throw 'No signature inputs'}
@{schema='GPU368_CERTIFICATE_VERIFICATION_V1';files=$rows;private_key_exported=$false;hardware_verified=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $ReportJson -Encoding UTF8
