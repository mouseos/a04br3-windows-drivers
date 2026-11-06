$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$um=Join-Path $kit 'Lib\winv6.3\um\arm'
& (Join-Path $root 'tools\ci\build-usb-bqhr.ps1')
$out=Join-Path $env:RUNNER_TEMP 'usb-bqhr-signed';if(Test-Path $out){throw 'output exists'}
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item (Join-Path $env:RUNNER_TEMP 'usb-bqhr\bqhr-observe.sys') $out
Copy-Item (Join-Path $env:RUNNER_TEMP 'usb-bqhr\bqhr-observe.inf') $out
$obj=Join-Path $env:RUNNER_TEMP 'usb-bqhr-uart.obj';$exe=Join-Path $out 'usb-bqhr-uart.exe'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\usb-bqhr-uart.c') -o $obj
if($LASTEXITCODE){throw 'BQHR UART compile failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib $obj (Join-Path $um 'kernel32.lib') "/out:$exe"
if($LASTEXITCODE){throw 'BQHR UART link failed'}
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=A04BR3 BQHR Observation Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
try {
 $sig=Join-Path $kit 'bin\x86\signtool.exe';$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
 Export-Certificate -Cert $cert -FilePath (Join-Path $out 'bqhr-observe-test.cer') | Out-Null
 foreach($f in @('bqhr-observe.sys','usb-bqhr-uart.exe')) {
  & $sig sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out $f)
  if($LASTEXITCODE){throw "sign $f failed"}
  $signature=Get-AuthenticodeSignature (Join-Path $out $f)
  if(!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw "signer mismatch $f"}
 }
 & $cat "/driver:$out" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'BQHR Inf2Cat failed'}
 $catalog=Join-Path $out 'bqhr-observe.cat'
 & $sig sign /fd SHA256 /sha1 $cert.Thumbprint /s My $catalog
 if($LASTEXITCODE){throw 'BQHR catalog signing failed'}
 $signature=Get-AuthenticodeSignature $catalog
 if(!$signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'BQHR catalog signer mismatch'}
 $files=@{};Get-ChildItem $out -File | ForEach-Object {$files[$_.Name]=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}
 [ordered]@{source_commit=(git rev-parse HEAD);signer_sha1=$cert.Thumbprint;files=$files;note='Self-signed BQHR read-only diagnostic; target trust and PnP unverified; no I2C access'} | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
} finally {Remove-Item (Join-Path 'Cert:\CurrentUser\My' $cert.Thumbprint) -ErrorAction SilentlyContinue}
