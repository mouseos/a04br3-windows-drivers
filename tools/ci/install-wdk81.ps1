$ErrorActionPreference = 'Stop'
$sdk = Join-Path $env:RUNNER_TEMP 'sdk81-setup.exe'
$wdk = Join-Path $env:RUNNER_TEMP 'wdk81-setup.exe'
& curl.exe --fail --location --retry 3 --output $sdk 'https://download.microsoft.com/download/B/0/C/B0C80BA3-8AD6-4958-810B-6882485230B5/standalonesdk/sdksetup.exe'
if ($LASTEXITCODE -ne 0) { throw 'SDK installer download failed' }
& curl.exe --fail --location --retry 3 --output $wdk 'https://download.microsoft.com/download/8/6/9/86925F0F-D57A-4BA4-8278-861B6876D78E/wdk/wdksetup.exe'
if ($LASTEXITCODE -ne 0) { throw 'WDK installer download failed' }
foreach ($file in @($sdk, $wdk)) {
    $sig = Get-AuthenticodeSignature $file
    Write-Host "$(Split-Path $file -Leaf): $($sig.Status), $($sig.SignerCertificate.Subject)"
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'Microsoft') { throw 'Installer signature verification failed' }
    $process = Start-Process -FilePath $file -ArgumentList '/quiet','/norestart' -Wait -PassThru
    Write-Host "$(Split-Path $file -Leaf) exit code $($process.ExitCode)"
    if ($process.ExitCode -ne 0) { throw 'Installer failed or requires reboot' }
}
