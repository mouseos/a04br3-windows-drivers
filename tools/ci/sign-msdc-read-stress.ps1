# Sign the read-only WinPE diagnostic with the matching storage test key.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$public = Join-Path $tempRoot 'a04br3-wdk81-probe\package-test-signed\a04test.cer'
$program = Join-Path $env:TEMP 'msdc-read-stress\msdc-read-stress.exe'
if (!(Test-Path $public) -or !(Test-Path $program)) { throw 'Storage certificate or stress executable missing' }
$thumb = (New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($public)).Thumbprint
$cert = Get-Item "Cert:\CurrentUser\My\$thumb"
if (!$cert.HasPrivateKey) { throw 'Storage test private key is unavailable' }
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $thumb /s My $program
if ($LASTEXITCODE -ne 0) { throw 'Storage stress signing failed' }
Get-FileHash -Algorithm SHA256 $program
