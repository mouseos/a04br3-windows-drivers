$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$source = Join-Path $tempRoot 'a04-wdf-touch'
$package = Join-Path $tempRoot 'a04-wdf-touch-signed'
if (Test-Path $package) { throw 'Signed touch package already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 Touch Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'a04touch.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'bu21025.sys')
if ($LASTEXITCODE -ne 0) { throw 'Touch SYS signing failed' }
Remove-Item (Join-Path $package 'bu21025.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'Touch catalog generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'bu21025.cat')
if ($LASTEXITCODE -ne 0) { throw 'Touch catalog signing failed' }
$status = Join-Path $tempRoot 'a04-touch-provider-status\touch-provider-status.exe'
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $status
if ($LASTEXITCODE -ne 0) { throw 'Touch PnP status signing failed' }
Get-FileHash -Algorithm SHA256 (Join-Path $package 'bu21025.sys'),(Join-Path $package 'bu21025.cat'),(Join-Path $package 'bu21025.inf') | Format-Table -AutoSize
Write-Host 'Experimental test-signed package only; not a production certificate.'
