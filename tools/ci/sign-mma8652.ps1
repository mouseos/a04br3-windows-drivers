# Research-only test signature; this does not grant RT production trust.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$source = Join-Path $tempRoot 'mma8652-build-only\package'
$package = if ($env:A04_MMA_SIGN_DIR) { $env:A04_MMA_SIGN_DIR } else { Join-Path $tempRoot 'mma8652-test-signed-r4' }
if (Test-Path $package) { throw 'Signed package already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 MMA8652 Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'mma8652.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mma8652.sys')
if ($LASTEXITCODE -ne 0) { throw 'SYS signing failed' }
Remove-Item (Join-Path $package 'mma8652.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Catalog generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mma8652-status.exe')
if ($LASTEXITCODE -ne 0) { throw 'Status tool signing failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mma8652-read.exe')
if ($LASTEXITCODE -ne 0) { throw 'Read tool signing failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mma8652.cat')
if ($LASTEXITCODE -ne 0) { throw 'Catalog signing failed' }
Get-AuthenticodeSignature (Join-Path $package 'mma8652.sys') | Format-List Status,StatusMessage,SignerCertificate
Get-FileHash -Algorithm SHA256 (Join-Path $package 'mma8652.sys'),(Join-Path $package 'mma8652.cat'),(Join-Path $package 'mma8652.inf') | Format-Table -AutoSize
Write-Host 'Research-only test signature; acceptance by Windows RT is unverified.'
