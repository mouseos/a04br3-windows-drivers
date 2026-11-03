$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq 'CN=A04BR3 Touch Development' -and $_.HasPrivateKey } | Sort-Object NotAfter -Descending | Select-Object -First 1
if (-not $cert) { throw 'Touch development certificate unavailable on this runner' }
$status = Join-Path $tempRoot 'a04-touch-provider-status\touch-provider-status.exe'
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My $status
if ($LASTEXITCODE -ne 0) { throw 'Touch PnP status signing failed' }
Get-FileHash -Algorithm SHA256 $status
