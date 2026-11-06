param([Parameter(Mandatory=$true)][string]$AuditExe,
      [Parameter(Mandatory=$true)][string]$ReleaseExe,
      [Parameter(Mandatory=$true)][string]$SenderExe,
      [string]$PnpExe,
      [string]$CopyExe,
      [string]$MapExe,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path $OutputDirectory){throw 'Output directory exists'}
foreach($p in @($AuditExe,$ReleaseExe,$SenderExe)){if(!(Test-Path -LiteralPath $p -PathType Leaf)){throw 'Input missing'}}
New-Item -ItemType Directory $OutputDirectory | Out-Null
Copy-Item -LiteralPath $AuditExe -Destination (Join-Path $OutputDirectory 'audit-volume-sys.exe')
Copy-Item -LiteralPath $ReleaseExe -Destination (Join-Path $OutputDirectory 'release-offline-volume.exe')
Copy-Item -LiteralPath $SenderExe -Destination (Join-Path $OutputDirectory 'uart-audit-stream.exe')
$names=@('audit-volume-sys.exe','release-offline-volume.exe','uart-audit-stream.exe')
if($PnpExe){if(!(Test-Path -LiteralPath $PnpExe -PathType Leaf)){throw 'PnP input missing'};Copy-Item -LiteralPath $PnpExe -Destination (Join-Path $OutputDirectory 'audit-pnp-inf.exe');$names+='audit-pnp-inf.exe'}
if($CopyExe){if(!(Test-Path -LiteralPath $CopyExe -PathType Leaf)){throw 'copy input missing'};Copy-Item -LiteralPath $CopyExe -Destination (Join-Path $OutputDirectory 'audit-copy-hives.exe');$names+='audit-copy-hives.exe'}
if($MapExe){if(!(Test-Path -LiteralPath $MapExe -PathType Leaf)){throw 'map input missing'};Copy-Item -LiteralPath $MapExe -Destination (Join-Path $OutputDirectory 'map-offline-windows.exe');$names+='map-offline-windows.exe'}
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Audit Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
try {
 Export-Certificate -Cert $cert -FilePath (Join-Path $OutputDirectory 'audit-test.cer') | Out-Null
 $sign=Join-Path "${env:ProgramFiles(x86)}\Windows Kits\8.1" 'bin\x86\signtool.exe'
 foreach($name in $names) {
  $p=Join-Path $OutputDirectory $name
  & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $p
  if($LASTEXITCODE){throw "signature failed: $name"}
  $sig=Get-AuthenticodeSignature -LiteralPath $p
  if(!$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'signer mismatch'}
 }
 $files=@{};foreach($f in (Get-ChildItem $OutputDirectory -File)){$files[$f.Name]=(Get-FileHash -Algorithm SHA256 $f.FullName).Hash.ToLowerInvariant()}
 [ordered]@{thumbprint=$cert.Thumbprint;files=$files;scope='test only; RT acceptance and integrated media unverified'} | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $OutputDirectory 'manifest.json')
} finally {Remove-Item -LiteralPath ("Cert:\CurrentUser\My\"+$cert.Thumbprint) -Force -ErrorAction SilentlyContinue}
