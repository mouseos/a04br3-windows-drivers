param([Parameter(Mandatory=$true)][string]$InputZip, [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$zip=(Resolve-Path $InputZip).Path
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$base=(Resolve-Path $OutputDirectory).Path
$dest=Join-Path $base 'package'
if(Test-Path $dest){throw 'output exists'}
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$sign=Join-Path $kit 'bin\x86\signtool.exe'
$cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 BU21025 Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $dest 'shared-touch-test.cer') | Out-Null
foreach($p in @(@('parent','mt8167-shared.sys','shared-parent.cat'),@('touch','bu21025.sys','bu21025.cat'))) {
 $d=Join-Path $dest $p[0];$sys=Join-Path $d $p[1];$catalog=Join-Path $d $p[2]
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $sys
 if($LASTEXITCODE -ne 0){throw 'SYS signing failed'}
 & $cat "/driver:$d" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE -ne 0){throw 'Catalog generation failed'}
 & $sign sign /fd SHA256 /sha1 $cert.Thumbprint /s My $catalog
 if($LASTEXITCODE -ne 0){throw 'Catalog signing failed'}
}
Get-ChildItem $dest -Recurse -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize
Compress-Archive -Path (Join-Path $dest '*') -DestinationPath (Join-Path $base 'signed.zip')
Write-Host 'TEST PACKAGE ONLY: target signature acceptance, PnP and recovery remain unverified.'
