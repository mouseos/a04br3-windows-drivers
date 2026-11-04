param(
 [Parameter(Mandatory=$true)][string]$UnsignedPackage,
 [Parameter(Mandatory=$true)][string]$SignedPackage
)
$ErrorActionPreference='Stop'
foreach($name in @('mt8167-toprgu.sys','mt8167-toprgu.inf','mt8167-toprgu.cat')) {
 if(!(Test-Path (Join-Path $UnsignedPackage $name) -PathType Leaf)) { throw "Missing $name" }
}
if(Test-Path $SignedPackage) { throw 'Signed package already exists' }
Copy-Item -Recurse $UnsignedPackage $SignedPackage
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe'
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 TOPRGU Early Test' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $SignedPackage 'mt8167-toprgu.cer') | Out-Null
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $SignedPackage 'mt8167-toprgu.sys')
if($LASTEXITCODE) { throw 'SYS sign failed' }
Remove-Item (Join-Path $SignedPackage 'mt8167-toprgu.cat')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$SignedPackage" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE) { throw 'CAT generation failed' }
& $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $SignedPackage 'mt8167-toprgu.cat')
if($LASTEXITCODE) { throw 'CAT sign failed' }
Write-Host "TEST_CERT_THUMBPRINT=$($cert.Thumbprint)"
Get-FileHash -Algorithm SHA256 (Join-Path $SignedPackage 'mt8167-toprgu.sys'),(Join-Path $SignedPackage 'mt8167-toprgu.cat'),(Join-Path $SignedPackage 'mt8167-toprgu.cer') | Format-List
