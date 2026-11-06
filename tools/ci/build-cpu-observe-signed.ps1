$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$env:A04_SHARED_BUTTON_D0_EXPERIMENT='1'
$env:A04_SHARED_TOUCH_D0_EXPERIMENT='1'
$env:A04_SHARED_APMIXED_RESOURCE='1'
$env:A04_SHARED_CPU_OBSERVE='1'
$env:A04_CPU_USB_OBSERVE='1'
& (Join-Path $root 'tools\ci\build-msdc-parent-probe.ps1')
& (Join-Path $root 'tools\ci\build-cpu-observe.ps1')
& (Join-Path $root 'tools\ci\build-usb-power-uart.ps1')
$out=Join-Path $env:RUNNER_TEMP 'cpu-observe-signed'
if(Test-Path $out){throw 'output exists'}
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item (Join-Path $env:RUNNER_TEMP 'a04-msdc-parent-probe\mt8167-shared.sys') $out
Copy-Item (Join-Path $root 'drivers\bus\mt8167-shared\prototype\shared-parent.inf') $out
Copy-Item (Join-Path $env:RUNNER_TEMP 'a04-cpu-observe\cpu-observe.exe') $out
Copy-Item (Join-Path $env:RUNNER_TEMP 'a04-usb-power-uart\usb-power-uart.exe') $out
Copy-Item (Join-Path $root 'tools\winpe\usb-power-observe.cmd') $out
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 CPU Observation Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
try {
 $sig=Join-Path $kit 'bin\x86\signtool.exe';$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
 Export-Certificate -Cert $cert -FilePath (Join-Path $out 'cpu-observe-test.cer') | Out-Null
 foreach($f in @('mt8167-shared.sys','cpu-observe.exe','usb-power-uart.exe')) {
  & $sig sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out $f)
  if($LASTEXITCODE){throw "sign $f failed"}
  $signature=Get-AuthenticodeSignature (Join-Path $out $f)
  if(!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw "signer mismatch $f"}
 }
 & $cat "/driver:$out" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 & $sig sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out 'shared-parent.cat')
 if($LASTEXITCODE){throw 'catalog signing failed'}
 $signature=Get-AuthenticodeSignature (Join-Path $out 'shared-parent.cat')
 if(!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'catalog signer mismatch'}
 Write-Output ("SIGNER="+$cert.Thumbprint)
} finally {Remove-Item (Join-Path 'Cert:\CurrentUser\My' $cert.Thumbprint) -ErrorAction SilentlyContinue}
