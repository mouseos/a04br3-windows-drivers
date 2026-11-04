param(
 [Parameter(Mandatory=$true)][string]$UnsignedToprgu,
 [Parameter(Mandatory=$true)][string]$UnsignedExe,
 [Parameter(Mandatory=$true)][string]$SignedPackage
)
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath $UnsignedExe -PathType Leaf)) { throw 'screen EXE missing' }
& (Join-Path $PSScriptRoot 'sign-toprgu-early.ps1') -UnsignedPackage $UnsignedToprgu -SignedPackage $SignedPackage
$cert=Get-PfxCertificate (Join-Path $SignedPackage 'mt8167-toprgu.cer')
$dest=Join-Path $SignedPackage 'uart-screen.exe'
Copy-Item -LiteralPath $UnsignedExe -Destination $dest
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My $dest
if($LASTEXITCODE) { throw 'screen tool signature failed' }
Get-FileHash -Algorithm SHA256 $dest | Format-List
