# Create a temporary development certificate on the Windows runner and sign
# only the generated diagnostic catalog. This does not make it trusted.
$ErrorActionPreference = 'Stop'
$package = Join-Path $env:RUNNER_TEMP 'a04br3-wdk81-probe\package'
$cat = Join-Path $package 'a04msdcprobe.cat'
if (-not (Test-Path $cat)) { throw 'Build the diagnostic package first' }
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Development Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'a04test.cer') | Out-Null
$signtool = Join-Path "${env:ProgramFiles(x86)}\Windows Kits\8.1" 'bin\x86\signtool.exe'
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $cat
if ($LASTEXITCODE -ne 0) { throw 'Test catalog signing failed' }
Write-Host 'The test certificate is untrusted by default. The private key remains on this runner.'
