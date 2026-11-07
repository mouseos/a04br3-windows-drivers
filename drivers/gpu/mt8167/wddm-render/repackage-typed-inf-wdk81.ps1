# SPDX-License-Identifier: MIT
# INF/CAT-only roll-forward. Never compile or re-sign SYS/DLL/client.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$AutoClientDirectory,
 [Parameter(Mandatory=$true)][string]$AutoClientManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Limited development package is default off'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Verify([string]$p,[string]$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $p -Algorithm SHA256).Hash -ne $sha){throw "Hash mismatch: $p"}}
$bp=Join-Path $BaselineDirectory 'signed-manifest.json';Verify $bp $BaselineManifestSha256;$b=Get-Content -Raw $bp|ConvertFrom-Json
$ap=Join-Path $AutoClientDirectory 'signed-client-manifest.json';Verify $ap $AutoClientManifestSha256;$a=Get-Content -Raw $ap|ConvertFrom-Json
if($b.signature_status -ne 'Valid' -or $a.signature_status -ne 'Valid' -or $b.signer_thumbprint -ne $CertificateThumbprint -or $a.signer_thumbprint -ne $CertificateThumbprint -or $a.runtime_verified){throw 'Unexpected input qualification or signer'}
foreach($r in $b.outputs){Verify (Join-Path $BaselineDirectory $r.name) $r.sha256}
Verify (Join-Path $AutoClientDirectory 'ge8300-d3d-clear.exe') $a.signed_sha256
Verify (Join-Path $AutoClientDirectory 'build-manifest.json') $a.build_manifest_sha256
$inf=Join-Path $PSScriptRoot 'ge8300-render.inf'
if((Get-Content -Raw $inf) -notmatch '(?m)^DriverVer=11/07/2026,0\.0\.3\.2\s*$'){throw 'Expected committed DriverVer 11/07/2026,0.0.3.2'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
# Inf2Cat uses UTC by default. Do not change the runner clock or backdate INF.
if([DateTime]::UtcNow.Date -lt [DateTime]'2026-11-07'){throw 'UTC date precedes committed INF date'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 $unchanged=@()
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','mt8167-development.cer')){
  $p=Join-Path $BaselineDirectory $name;Copy-Item $p $out;$unchanged+=@{name=$name;sha256=(Get-FileHash $p -Algorithm SHA256).Hash;origin='signed 0f8f68d2 package'}
 }
 Copy-Item $inf $out
 Copy-Item (Join-Path $AutoClientDirectory 'ge8300-d3d-clear.exe') $out
 $unchanged+=@{name='ge8300-d3d-clear.exe';sha256=$a.signed_sha256;origin=$a.source_commit}
 Copy-Item $bp "$out\baseline-signed-manifest.json"
 Copy-Item (Join-Path $BaselineDirectory 'linked-manifest.json') $out
 Copy-Item $ap "$out\signed-client-manifest.json"
 Copy-Item (Join-Path $AutoClientDirectory 'build-manifest.json') "$out\client-build-manifest.json"
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe';$sign=Join-Path $kit 'bin\x64\signtool.exe'
 & $cat "/driver:$out" /os:6_3_ARM
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 $store=@();if(Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"){$store+='/sm'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @store (Join-Path $out 'ge8300-render.cat')
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($r in $unchanged){Verify (Join-Path $out $r.name) $r.sha256}
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-render.cat')){
  $sig=Get-AuthenticodeSignature (Join-Path $out $name)
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw "Signature not Valid: $name"}
 }
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){
  & $sign verify /pa /c (Join-Path $out 'ge8300-render.cat') (Join-Path $out $name)
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 @{package_source_commit=(& git -C $root rev-parse HEAD);inf_driverver='11/07/2026,0.0.3.2';baseline_manifest_sha256=$BaselineManifestSha256;auto_client_manifest_sha256=$AutoClientManifestSha256;
  source_runtime=$b.source_runtime;client_source_commit=$a.source_commit;unchanged_files=$unchanged;compiled=$false;resigned_images=$false;signed_only='ge8300-render.cat';signature_status='Valid';signer_thumbprint=$CertificateThumbprint;
  fixed_arguments='--hardware=PowerVR';default_off=$true;runtime_verified=$false;installed=$false;standard_profile_qualified=$false;
  required_parent_commit=$b.required_parent_commit;required_acpi_commit=$b.required_acpi_commit;display_policy=$b.display_policy;
  rank_reason='Old KMDF 2.5 date 11/07/2026 outranks old render 3.1 date 11/06/2026; equal 11/07 date plus higher render version 0.0.3.2 avoids date-order disadvantage. Other PnP rank factors remain applicable.';
  outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-render.cat','mt8167-development.cer','ge8300-d3d-clear.exe')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 14|Set-Content "$out\signed-manifest.json"
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
