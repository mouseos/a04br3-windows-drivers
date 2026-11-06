param([Parameter(Mandatory=$true)][string]$BackupExe,
      [Parameter(Mandatory=$true)][string]$ReleaseExe,
      [string]$HiveApplyExe,
      [string]$BaselineGuardExe,
      [string]$SignatureCheckExe,
      [string]$PnpBaselineExe,
      [string]$StageDriversExe,
      [string]$PublishDriversExe,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $OutputDirectory){throw 'Output directory already exists'}
New-Item -ItemType Directory $OutputDirectory | Out-Null
foreach($file in @($BackupExe,$ReleaseExe)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw 'Input executable missing'}}
Copy-Item -LiteralPath $BackupExe -Destination (Join-Path $OutputDirectory 'offline-backup.exe')
Copy-Item -LiteralPath $ReleaseExe -Destination (Join-Path $OutputDirectory 'release-offline-volume.exe')
$names=@('offline-backup.exe','release-offline-volume.exe')
if($HiveApplyExe){if(!(Test-Path -LiteralPath $HiveApplyExe -PathType Leaf)){throw 'Hive apply executable missing'};Copy-Item -LiteralPath $HiveApplyExe -Destination (Join-Path $OutputDirectory 'offline-hive-apply.exe');$names+= 'offline-hive-apply.exe'}
if($BaselineGuardExe){if(!(Test-Path -LiteralPath $BaselineGuardExe -PathType Leaf)){throw 'Baseline guard executable missing'};Copy-Item -LiteralPath $BaselineGuardExe -Destination (Join-Path $OutputDirectory 'offline-baseline-guard.exe');$names+='offline-baseline-guard.exe'}
if($SignatureCheckExe){if(!(Test-Path -LiteralPath $SignatureCheckExe -PathType Leaf)){throw 'Signature checker executable missing'};Copy-Item -LiteralPath $SignatureCheckExe -Destination (Join-Path $OutputDirectory 'offline-signature-check.exe');$names+='offline-signature-check.exe'}
if($PnpBaselineExe){if(!(Test-Path -LiteralPath $PnpBaselineExe -PathType Leaf)){throw 'PnP baseline executable missing'};Copy-Item -LiteralPath $PnpBaselineExe -Destination (Join-Path $OutputDirectory 'offline-pnp-baseline.exe');$names+='offline-pnp-baseline.exe'}
if($StageDriversExe){if(!(Test-Path -LiteralPath $StageDriversExe -PathType Leaf)){throw 'Stage drivers executable missing'};Copy-Item -LiteralPath $StageDriversExe -Destination (Join-Path $OutputDirectory 'offline-stage-drivers.exe');$names+='offline-stage-drivers.exe'}
if($PublishDriversExe){if(!(Test-Path -LiteralPath $PublishDriversExe -PathType Leaf)){throw 'Publish drivers executable missing'};Copy-Item -LiteralPath $PublishDriversExe -Destination (Join-Path $OutputDirectory 'offline-publish-drivers.exe');$names+='offline-publish-drivers.exe'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe'
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Offline Maintenance Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
try {
 Export-Certificate -Cert $cert -FilePath (Join-Path $OutputDirectory 'maintenance-test.cer') | Out-Null
 foreach($name in $names){
  $p=Join-Path $OutputDirectory $name
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $p
  if($LASTEXITCODE){throw "Signing failed: $name"}
  $sig=Get-AuthenticodeSignature -LiteralPath $p
  if(!$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw "Signer mismatch: $name"}
 }
 $items=Get-ChildItem -LiteralPath $OutputDirectory -File | Get-FileHash -Algorithm SHA256
 $result=[ordered]@{thumbprint=$cert.Thumbprint; files=@{}; scope='test only; target signature acceptance unverified'}
 foreach($item in $items){$result.files[(Split-Path $item.Path -Leaf)]=$item.Hash.ToLowerInvariant()}
 $result | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $OutputDirectory 'manifest.json')
 Write-Host "TEST_CERT_THUMBPRINT=$($cert.Thumbprint)"
} finally {
 Remove-Item -LiteralPath ("Cert:\CurrentUser\My\"+$cert.Thumbprint) -Force -ErrorAction SilentlyContinue
}
