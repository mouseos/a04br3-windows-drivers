# Public selected-identity verifier, derived from verify-dwm-certificates.ps1.
# No key import/export, store modification, install or device call.
param([Parameter(Mandatory=$true)][string]$PathsJson,
 [Parameter(Mandatory=$true)][string]$ExpectedThumbprint,
 [Parameter(Mandatory=$true)][string]$ReportJson)
$ErrorActionPreference='Stop'
if($ExpectedThumbprint -cnotmatch '\A[0-9A-Fa-f]{40}\z'){throw 'Thumbprint must be exactly 40 ASCII hex characters'}
$thumb=$ExpectedThumbprint.ToUpperInvariant()
$rows=@()
$decoded=Get-Content -LiteralPath $PathsJson -Raw|ConvertFrom-Json
foreach($file in $decoded){
 if($file -isnot [string]){throw 'Individual signature path required'}
 $sig=Get-AuthenticodeSignature -LiteralPath $file
 if($sig.Status -ne 'Valid' -or !$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -cne $thumb){throw "Signature identity/trust failed: $file"}
 $rows+=@{path=$file;status=[string]$sig.Status;signer_thumbprint=$sig.SignerCertificate.Thumbprint;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()}
}
if(!$rows.Count){throw 'No signature inputs'}
@{schema='PUBLIC_SIGNING_CERTIFICATE_VERIFICATION_V1';expected_thumbprint=$thumb;files=$rows;private_key_exported=$false;hardware_verified=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $ReportJson -Encoding UTF8
