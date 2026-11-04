# Produce a paired experimental GE8300 provider/display package using one
# temporary test certificate. This does not establish trust on any device.
$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$provider = Join-Path $tempRoot 'mt8167-ge8300-provider\package'
$display = Join-Path $tempRoot 'mt8167-display-package'
$status = Join-Path $tempRoot 'mt8167-ge8300-status'
$target = Join-Path $tempRoot 'mt8167-ge8300-display-test-signed'
foreach ($path in @((Join-Path $provider 'mt8167-ge8300.cat'),
                    (Join-Path $display 'mt8167-display.cat'),
                    (Join-Path $status 'ge8300-status.exe'),
                    (Join-Path $status 'ge8300-pnp-status.exe'),
                    (Join-Path $status 'ge8300-service-status.exe'),
                    (Join-Path $status 'ge8300-copy-status.exe'),
                    (Join-Path $status 'display-pnp-status.exe'))) {
    if (!(Test-Path $path)) { throw "Required build input missing: $path" }
}
if (Test-Path $target) { throw 'Test package output already exists' }
New-Item -ItemType Directory $target | Out-Null
$destProvider = Join-Path $target 'ge8300'
$destDisplay = Join-Path $target 'display'
Copy-Item -Recurse $provider $destProvider
Copy-Item -Recurse $display $destDisplay
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 Display Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $target 'mt8167-display-test.cer') | Out-Null
$sign = Join-Path $kit 'bin\x86\signtool.exe'
foreach ($entry in @(@($destProvider,'mt8167-ge8300'),@($destDisplay,'mt8167-display'))) {
    $directory=$entry[0]; $name=$entry[1]
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $directory "$name.sys")
    if ($LASTEXITCODE -ne 0) { throw "Test signing failed: $name.sys" }
    Remove-Item (Join-Path $directory "$name.cat")
    & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$directory" /os:6_3_ARM /uselocaltime
    if ($LASTEXITCODE -ne 0) { throw "Catalog generation failed: $name" }
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $directory "$name.cat")
    if ($LASTEXITCODE -ne 0) { throw "Test signing failed: $name.cat" }
}
foreach ($name in @('ge8300-status.exe','ge8300-pnp-status.exe','ge8300-service-status.exe','ge8300-copy-status.exe','display-pnp-status.exe')) {
    Copy-Item (Join-Path $status $name) $destProvider
    & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $destProvider $name)
    if ($LASTEXITCODE -ne 0) { throw "Test signing failed: $name" }
}
Write-Host "Test-signed packages in $target; not device-validated."
