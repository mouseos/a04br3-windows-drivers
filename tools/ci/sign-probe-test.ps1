# Create a temporary development certificate on this Windows runner and sign
# a copy of the diagnostic SYS and its regenerated catalog. This does not
# make the certificate trusted and does not change the unsigned baseline.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base = Join-Path $tempRoot 'a04br3-wdk81-probe'
$source = Join-Path $base 'package'
$package = Join-Path $base 'package-test-signed'
$bootPackage = Test-Path (Join-Path $source 'mt8167-msdc.inf')
$passivePackage = Test-Path (Join-Path $source 'mt8167-msdc-readonly.inf')
$scratchPackage = Test-Path (Join-Path $source 'mt8167-msdc-scratch.inf')
$sysName = if ($bootPackage -or $passivePackage -or $scratchPackage) { 'mt8167-msdc.sys' } else { 'a04msdcprobe.sys' }
$catName = if ($bootPackage) { 'mt8167-msdc.cat' } elseif ($scratchPackage) { 'mt8167-msdc-scratch.cat' } elseif ($passivePackage) { 'mt8167-msdc-readonly.cat' } else { 'a04msdcprobe.cat' }
if (-not (Test-Path (Join-Path $source $catName))) { throw 'Build the storage package first' }
if (Test-Path $package) { throw 'Test package directory already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Development Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'a04test.cer') | Out-Null
$signtool = Join-Path $kit 'bin\x86\signtool.exe'
$driver = Join-Path $package $sysName
$catalog = Join-Path $package $catName
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $driver
if ($LASTEXITCODE -ne 0) { throw 'Test driver signing failed' }
Remove-Item $catalog
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Test catalog generation failed' }
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /s My $catalog
if ($LASTEXITCODE -ne 0) { throw 'Test catalog signing failed' }
Write-Host "Untrusted test package: $package. The private key remains on this runner."
