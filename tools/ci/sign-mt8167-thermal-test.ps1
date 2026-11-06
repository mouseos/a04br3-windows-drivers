# Self-signed test package only. It does not establish target trust or safety.
$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$tempRoot=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP}
$source=Join-Path $tempRoot 'a04br3-thermal-diagnostic'
$target=Join-Path $tempRoot 'a04br3-thermal-shared-test-signed'
if(Test-Path $target){throw 'Refusing to overwrite existing package'}
New-Item -ItemType Directory $target | Out-Null
foreach($name in @('mt8167-thermal.sys','mt8167-thermal.inf')){Copy-Item (Join-Path $source $name) $target}
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 Thermal Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
try {
 Export-Certificate -Cert $cert -FilePath (Join-Path $target 'mt8167-thermal-test.cer') | Out-Null
 $sign=Join-Path $kit 'bin\x86\signtool.exe'
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $target 'mt8167-thermal.sys')
 if($LASTEXITCODE){throw 'SYS signing failed'}
 & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$target" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'catalog generation failed'}
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $target 'mt8167-thermal.cat')
 if($LASTEXITCODE){throw 'CAT signing failed'}
 Get-AuthenticodeSignature (Join-Path $target 'mt8167-thermal.sys') | Format-List Status,StatusMessage
 Get-AuthenticodeSignature (Join-Path $target 'mt8167-thermal.cat') | Format-List Status,StatusMessage
} finally { Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)" -ErrorAction SilentlyContinue }
Write-Host 'Test-signed only: certificate trust, device loading, handoff and temperature remain unverified.'
