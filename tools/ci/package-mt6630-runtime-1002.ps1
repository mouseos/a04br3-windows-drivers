# Repackage the already signed 37a5bb1 SYS with a distinct INF version.
# No compile, device operation, certificate generation or parent rebuild.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$SignedSys,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [ValidatePattern('^[a-fA-F0-9]{40}$')]
 [string]$CertificateThumbprint='2655FC89A7082D3BAB3F26EC556EE8A3C52F4358'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$expectedSys='77b53ac0b902d5d6c864247707e9f319585ebb64d3a17b8a14696c8153e1a821'
function Get-Sha256([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
if((Get-Sha256 $SignedSys) -ne $expectedSys){throw 'Expected the signed 37a5bb1 SYS; old f17 SYS is not accepted'}
$inf=Join-Path $root 'drivers\network\mt6630\mt6630-runtime.inf'
$driverVer='11/06/2026,2026.11.6.1002'
$infText=[IO.File]::ReadAllText($inf)
if([regex]::Matches($infText,'(?m)^DriverVer=[^\r\n]+').Count -ne 1 -or
   $infText -notmatch '(?m)^DriverVer=11/06/2026,2026\.11\.6\.1002\r?$'){throw 'Source INF must be version 1002'}
$sourceCommit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE){throw 'Source commit unavailable'}
$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty){throw 'Use a clean committed source checkout'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'OutputDirectory must be new'}
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe'
$inf2cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$cert.HasPrivateKey){throw 'Existing certificate private key required for catalog signing'}
$package=Join-Path $out 'package'
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath $SignedSys -Destination (Join-Path $package 'mt6630-runtime.sys')
Copy-Item -LiteralPath $inf -Destination (Join-Path $package 'mt6630-runtime.inf')
if((Get-Sha256 (Join-Path $package 'mt6630-runtime.sys')) -ne $expectedSys){throw 'Copied SYS hash mismatch'}
$work=Join-Path $out 'work'
New-Item -ItemType Directory -Path $work | Out-Null
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
try {
 $env:TEMP=$work;$env:TMP=$work;$env:RUNNER_TEMP=$work
 # Fresh directory contains no old CAT. Catalog covers the new INF and signed SYS.
 & $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint (Join-Path $package 'mt6630-runtime.cat')
 if($LASTEXITCODE){throw 'Catalog signing failed'}
} finally {
 $env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner
}
if((Get-Sha256 (Join-Path $package 'mt6630-runtime.sys')) -ne $expectedSys){throw 'SYS changed during packaging'}
Export-Certificate -Cert $cert -FilePath (Join-Path $out 'catalog-public.cer') | Out-Null
$files=@(Get-ChildItem -LiteralPath $package -File | Sort-Object Name | ForEach-Object {
 [ordered]@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Get-Sha256 $_.FullName)}
})
[ordered]@{
 status='repackaged-signed-not-device-validated';source_commit=$sourceCommit
 binary_source_commit='37a5bb1';binary_rebuilt=$false
 driver_ver=$driverVer;expected_installed_sys_sha256=$expectedSys
 rejected_old_sys_sha256='d11e86bc728f0cd5d519becbd6bf7f328883a84672cc6c6678608a65742182af'
 source_inf_sha256=(Get-Sha256 $inf);catalog_regenerated=$true
 catalog_certificate_thumbprint=$cert.Thumbprint;package_files=$files
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
Write-Output "Version 1002 candidate: $package. Confirm installed INF version AND SYS hash before judging NDIS results."
