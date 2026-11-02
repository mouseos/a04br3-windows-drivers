# Create a temporary development certificate on this Windows runner and sign
# a copy of the diagnostic SYS and its regenerated catalog. This does not
# make the certificate trusted and does not change the unsigned baseline.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$base = Join-Path $env:RUNNER_TEMP 'a04br3-wdk81-probe'
$source = Join-Path $base 'package'
$package = Join-Path $base 'package-test-signed'
if (-not (Test-Path (Join-Path $source 'a04msdcprobe.cat'))) { throw 'Build the diagnostic package first' }
if (Test-Path $package) { throw 'Test package directory already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Development Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'a04test.cer') | Out-Null
$signtool = Join-Path $kit 'bin\x86\signtool.exe'
$driver = Join-Path $package 'a04msdcprobe.sys'
$catalog = Join-Path $package 'a04msdcprobe.cat'
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $driver
if ($LASTEXITCODE -ne 0) { throw 'Test driver signing failed' }
Remove-Item $catalog
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Test catalog generation failed' }
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $catalog
if ($LASTEXITCODE -ne 0) { throw 'Test catalog signing failed' }
Write-Host "Untrusted test package: $package. The private key remains on this runner."
