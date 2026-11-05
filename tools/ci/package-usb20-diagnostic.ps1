param(
 [Parameter(Mandatory=$true)][string]$Driver,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$SigningThumbprint
)
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inputDriver=Resolve-Path $Driver
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$out=(Resolve-Path $OutputDirectory).Path
Copy-Item $inputDriver (Join-Path $out 'mt8167-usb20.sys') -Force
Copy-Item (Join-Path $root 'drivers\usb\mt8167-usb20\mt8167-usb20.inf') $out -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM /uselocaltime
if($LASTEXITCODE -ne 0) { throw 'Inf2Cat failed' }
if($SigningThumbprint) {
 if($SigningThumbprint -notmatch '^[0-9a-fA-F]{40}$') { throw 'Invalid certificate thumbprint' }
 $sign=Join-Path $kit 'bin\x86\signtool.exe'
 foreach($file in @('mt8167-usb20.sys','mt8167-usb20.cat')) {
  & $sign sign /fd SHA256 /sha1 $SigningThumbprint /s My (Join-Path $out $file)
  if($LASTEXITCODE -ne 0) { throw "Signing failed: $file" }
 }
}
Write-Host 'Offline diagnostic fixture only: no target ownership or Windows RT signing policy has been validated.'
