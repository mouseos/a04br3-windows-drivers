# SPDX-License-Identifier: MIT
# Seal already-linked immutable artifacts. Does not install, reboot or access hardware.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$LinkedDirectory,
 [Parameter(Mandatory=$true)][string]$LinkedManifestSha256,
 [Parameter(Mandatory=$true)][string]$Firmware,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Experimental render package is default off'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$commit=(& git -C $root rev-parse HEAD)
$manifestPath=Join-Path $LinkedDirectory 'linked-manifest.json'
if($LinkedManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash $manifestPath -Algorithm SHA256).Hash -ne $LinkedManifestSha256){throw 'Fixed linked provenance mismatch'}
$m=Get-Content -Raw $manifestPath|ConvertFrom-Json
if(($m.compilation_performed -and !@($m.additional_compile_units).Count) -or !$m.default_off -or $m.runtime_verified -or $m.signed -or $m.installed){throw 'Expected unsigned link-only candidate, not a runtime qualification claim'}
foreach($name in @('ge8300-render.sys','ge8300-umd.dll')){
 $record=@($m.outputs|Where-Object {$_.name -eq $name})
 if($record.Count -ne 1 -or (Get-FileHash (Join-Path $LinkedDirectory $name) -Algorithm SHA256).Hash -ne $record[0].sha256){throw 'Linked artifact hash mismatch'}
}
if($m.required_parent_commit -notmatch '^[0-9a-f]{40}$' -or $m.required_acpi_commit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed full parent/ACPI dependency commits required'}
if((Get-FileHash $Firmware -Algorithm SHA256).Hash -ne '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'){throw 'Require exact source-compatible existing GE8300 firmware'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Use a fresh output directory inside checkout'}
$machineStore=Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"
$certPath=if($machineStore){"Cert:\LocalMachine\My\$CertificateThumbprint"}else{"Cert:\CurrentUser\My\$CertificateThumbprint"}
$cert=Get-Item $certPath
$storeFlags=@();if($machineStore){$storeFlags+='/sm'}
if(!$cert.HasPrivateKey){throw 'Signing identity unavailable'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x64\signtool.exe';$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Copy-Item (Join-Path $PSScriptRoot 'ge8300-render.inf') $out
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll')){Copy-Item (Join-Path $LinkedDirectory $name) $out}
 Copy-Item $Firmware (Join-Path $out 'ge8300-mips.fw')
 Copy-Item $manifestPath (Join-Path $out 'linked-manifest.json')
 $unsigned=@('ge8300-render.sys','ge8300-umd.dll')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll')){
  & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @storeFlags (Join-Path $out $name)
  if($LASTEXITCODE){throw 'Image signing failed'}
 }
 & $cat "/driver:$out" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @storeFlags (Join-Path $out 'ge8300-render.cat')
 if($LASTEXITCODE){throw 'Catalog signing failed'}
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat')){
  $sig=Get-AuthenticodeSignature (Join-Path $out $name)
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw "Final signature not Valid: $name ($($sig.Status))"}
 }
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')){
  & $sign verify /pa /c (Join-Path $out 'ge8300-render.cat') (Join-Path $out $name)
  if($LASTEXITCODE){throw 'Final CAT membership verification failed'}
 }
 Export-Certificate -Cert $cert -FilePath (Join-Path $out 'mt8167-development.cer')|Out-Null
 @{package_source_commit=$commit;inf_driverver='11/06/2026,0.0.3.1';linked_manifest_sha256=$LinkedManifestSha256;
  source_runtime=$m;unsigned_images=$unsigned;signer_thumbprint=$CertificateThumbprint;signature_status='Valid';
  default_off=$true;installed=$false;runtime_verified=$false;wddm_conformance_qualified=$false;
  required_parent_commit=$m.required_parent_commit;required_acpi_commit=$m.required_acpi_commit;display_policy='MOU0007 CPU present; never concurrent KMDF GPU IRQ owner';
  limitations=@('Standard runtime negotiation and limited operation profile require runtime validation','WDDM1.2 mandatory engine-TDR/preemption/GDI contracts remain unqualified','NoDMA stop remains unqualified; do not auto-install');
  outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-render.cat','mt8167-development.cer')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 12|Set-Content (Join-Path $out 'signed-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
