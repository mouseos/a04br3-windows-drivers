# Test-sign the already-built GE8300 provider and status utility with a
# temporary runner certificate. This does not establish device trust.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$source = Join-Path $tempRoot 'mt8167-ge8300-provider\package'
$sourceStatus = Join-Path $tempRoot 'mt8167-ge8300-status\ge8300-status.exe'
$sourcePnp = Join-Path $tempRoot 'mt8167-ge8300-status\ge8300-pnp-status.exe'
$package = Join-Path $tempRoot 'mt8167-ge8300-test-signed'
if (-not (Test-Path (Join-Path $source 'mt8167-ge8300.cat')) -or -not (Test-Path $sourceStatus) -or -not (Test-Path $sourcePnp)) { throw 'Build provider and status tools first' }
if (Test-Path $package) { throw 'Test package directory already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 GE8300 Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'ge8300-test.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
$sys = Join-Path $package 'mt8167-ge8300.sys'
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $sys
if ($LASTEXITCODE -ne 0) { throw 'GE8300 SYS signing failed' }
Remove-Item (Join-Path $package 'mt8167-ge8300.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'GE8300 catalog generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mt8167-ge8300.cat')
if ($LASTEXITCODE -ne 0) { throw 'GE8300 catalog signing failed' }
Copy-Item $sourceStatus (Join-Path $package 'ge8300-status.exe')
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'ge8300-status.exe')
if ($LASTEXITCODE -ne 0) { throw 'GE8300 status signing failed' }
Copy-Item $sourcePnp (Join-Path $package 'ge8300-pnp-status.exe')
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'ge8300-pnp-status.exe')
if ($LASTEXITCODE -ne 0) { throw 'GE8300 PnP status signing failed' }
$vendorStatus = Join-Path $tempRoot 'mt8167-ge8300-status\ge8300-vendor-stage-status.exe'
if (Test-Path $vendorStatus) {
    Copy-Item $vendorStatus (Join-Path $package 'ge8300-vendor-stage-status.exe')
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'ge8300-vendor-stage-status.exe')
    if ($LASTEXITCODE -ne 0) { throw 'GE8300 vendor stage status signing failed' }
}
$vendorStartStatus = Join-Path $tempRoot 'mt8167-ge8300-status\ge8300-vendor-start-status.exe'
if (Test-Path $vendorStartStatus) {
    Copy-Item $vendorStartStatus (Join-Path $package 'ge8300-vendor-start-status.exe')
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'ge8300-vendor-start-status.exe')
    if ($LASTEXITCODE -ne 0) { throw 'GE8300 vendor start status signing failed' }
}
Write-Host "Test-signed GE8300 package: $package. The private key remains on this runner."
