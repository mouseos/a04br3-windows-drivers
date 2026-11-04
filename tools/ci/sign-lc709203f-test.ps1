# Produce a self-signed diagnostic package. This does not establish trust.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$source = Join-Path $tempRoot 'lc709203f-build-only\package'
$target = Join-Path $tempRoot 'lc709203f-test-signed'
if (Test-Path $target) { throw 'Test package output already exists' }
Copy-Item -Recurse $source $target
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=LC709203F Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $target 'lc709203f-test.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
foreach ($name in @('lc709203f.sys','lc709203f-status.exe','lc709203f-pnp-status.exe')) {
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $target $name)
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $name" }
}
Remove-Item (Join-Path $target 'lc709203f.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$target" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Battery catalog generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $target 'lc709203f.cat')
if ($LASTEXITCODE -ne 0) { throw 'Battery catalog signing failed' }
Write-Host "Test-signed LC709203F package in $target; not device-validated."
