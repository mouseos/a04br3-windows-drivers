param(
 [Parameter(Mandatory=$true)][string]$SourceWim,
 [Parameter(Mandatory=$true)][string]$OutputWim,
 [Parameter(Mandatory=$true)][string]$DriverPackage,
 [Parameter(Mandatory=$true)][string]$MountDirectory,
 [Parameter(Mandatory=$true)][string]$SourceSha256,
 [Parameter(Mandatory=$true)][string]$SysSha256
)
$ErrorActionPreference='Stop'
$sys=Join-Path $DriverPackage 'mt8167-toprgu.sys'
$cat=Join-Path $DriverPackage 'mt8167-toprgu.cat'
$inf=Join-Path $DriverPackage 'mt8167-toprgu.inf'
$cer=Join-Path $DriverPackage 'mt8167-toprgu.cer'
foreach($f in @($SourceWim,$sys,$cat,$inf,$cer)) { if(!(Test-Path -LiteralPath $f -PathType Leaf)) { throw "Missing: $f" } }
if((Get-FileHash $SourceWim -Algorithm SHA256).Hash -ne $SourceSha256) { throw 'WIM hash mismatch' }
if((Get-FileHash $sys -Algorithm SHA256).Hash -ne $SysSha256) { throw 'SYS hash mismatch' }
if((Test-Path $OutputWim) -or (Test-Path $MountDirectory) -or (Test-Path 'HKLM:\A04BR3OFF')) { throw 'Output, mount or hive name already exists' }
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($cer)
$thumb=$cert.Thumbprint
$mounted=$false; $loaded=$false
try {
 foreach($f in @($sys,$cat)) {
  $sig=Get-AuthenticodeSignature $f
  if(!$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -ne $thumb -or $sig.Status -notin @('Valid','UnknownError')) { throw "Missing or unexpected signature: $f $($sig.Status)" }
  Write-Host "SIGNATURE_STATUS $f $($sig.Status) $($sig.StatusMessage)"
 }
 # A serialized certificate has the registry certificate-store Blob format.
 # No host certificate trust is modified. Offline chain validation is separate.
 $blob=$cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::SerializedCert)
 New-Item -ItemType Directory -Path $MountDirectory | Out-Null
 Copy-Item -LiteralPath $SourceWim -Destination $OutputWim
 & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory" | Out-Host
 if($LASTEXITCODE) { throw 'WIM mount failed' }; $mounted=$true
 & dism.exe "/Image:$MountDirectory" /Add-Driver "/Driver:$inf" /ForceUnsigned | Out-Host
 if($LASTEXITCODE) { throw 'DISM driver injection failed' }
 & reg.exe load HKLM\A04BR3OFF (Join-Path $MountDirectory 'Windows\System32\config\SOFTWARE') | Out-Host
 if($LASTEXITCODE) { throw 'Offline SOFTWARE hive load failed' }; $loaded=$true
 $hex=([BitConverter]::ToString($blob)).Replace("-","")
 foreach($store in @('Root','TrustedPublisher')) {
  $dst="HKLM\A04BR3OFF\Microsoft\SystemCertificates\$store\Certificates\$thumb"
  & reg.exe add $dst /v Blob /t REG_BINARY /d $hex /f | Out-Host
  if($LASTEXITCODE) { throw "Offline $store import failed" }
  & reg.exe query $dst /v Blob | Out-Null
  if($LASTEXITCODE) { throw "Offline $store validation failed" }
  Write-Host "OFFLINE_CERT $store $thumb"
 }
 & reg.exe unload HKLM\A04BR3OFF | Out-Host
 if($LASTEXITCODE) { throw 'Offline SOFTWARE hive unload failed' }; $loaded=$false
 & dism.exe "/Image:$MountDirectory" /Get-Drivers /Format:Table | Out-Host
 if($LASTEXITCODE) { throw 'DISM inventory failed' }
 & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit | Out-Host
 if($LASTEXITCODE) { throw 'WIM commit failed' }; $mounted=$false
 Write-Host "TEST_CERT_THUMBPRINT=$thumb"
 Get-FileHash $OutputWim -Algorithm SHA256 | Format-List
} finally {
 if($loaded) { & reg.exe unload HKLM\A04BR3OFF | Out-Host }
 if($mounted) { & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard | Out-Host }
}
