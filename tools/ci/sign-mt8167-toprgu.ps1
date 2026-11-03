$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$source = Join-Path $tempRoot 'mt8167-toprgu'
$package = Join-Path $tempRoot 'mt8167-toprgu-signed'
if (Test-Path $package) { throw 'Signed TOPRGU package already exists' }
Copy-Item -Recurse $source $package
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 TOPRGU Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $package 'mt8167-toprgu.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mt8167-toprgu.sys')
if ($LASTEXITCODE -ne 0) { throw 'TOPRGU SYS signing failed' }
Remove-Item (Join-Path $package 'mt8167-toprgu.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'TOPRGU catalog generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $package 'mt8167-toprgu.cat')
if ($LASTEXITCODE -ne 0) { throw 'TOPRGU catalog signing failed' }
Get-FileHash -Algorithm SHA256 (Join-Path $package 'mt8167-toprgu.sys'),(Join-Path $package 'mt8167-toprgu.cat'),(Join-Path $package 'mt8167-toprgu.inf') | Format-Table -AutoSize
Write-Host 'Test-signed TOPRGU prototype; recovery ownership and sleep remain unverified.'
