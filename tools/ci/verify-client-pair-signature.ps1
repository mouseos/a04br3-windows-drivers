param([Parameter(Mandatory=$true)][string]$Image,[Parameter(Mandatory=$true)][string]$Report)
$ErrorActionPreference='Stop'
$s=Get-AuthenticodeSignature $Image
if([string]$s.Status -cne 'Valid' -or !$s.SignerCertificate -or $s.SignerCertificate.Thumbprint -cne 'BCBF6C456C6F0DD0818434763B189488FBF13570'){throw 'Actual BCBF Valid signature required'}
@{path=$Image;sha256=(Get-FileHash $Image -Algorithm SHA256).Hash.ToLowerInvariant();status=[string]$s.Status;thumbprint=$s.SignerCertificate.Thumbprint}|ConvertTo-Json|Set-Content -Encoding UTF8 $Report
