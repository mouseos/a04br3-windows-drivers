# SPDX-License-Identifier: MIT
# Signed kernel and firmware are preserved byte-for-byte. No installation.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$NativeDirectory,
 [Parameter(Mandatory=$true)][string]$NativeManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit limited development opt-in required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
function Verify($path,$sha){if($sha -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $sha){throw "Input hash mismatch: $path"}}
$bp=Join-Path $BaselineDirectory 'signed-manifest.json';Verify $bp $BaselineManifestSha256;$b=Get-Content -Raw $bp|ConvertFrom-Json
if($b.inf_driverver -ne '11/07/2026,0.0.3.13' -or $b.signature_status -ne 'Valid' -or $b.signer_thumbprint -ne $CertificateThumbprint){throw 'Signed 3.13 baseline required'}
foreach($f in $b.outputs){Verify (Join-Path $BaselineDirectory $f.name) $f.sha256}
$np=Join-Path $NativeDirectory 'signed-manifest.json';Verify $np $NativeManifestSha256;$n=Get-Content -Raw $np|ConvertFrom-Json
if($n.signature_status -ne 'Valid' -or $n.signer_thumbprint -ne $CertificateThumbprint -or !$n.limited_development_profile -or $n.standard_profile_qualified -or $n.kernel_changed -or $n.pipeline -ne '10_0'){throw 'Expected signed limited native development package'}
foreach($f in $n.outputs){Verify (Join-Path $NativeDirectory $f.name) $f.sha256}
$fixed=@{'ge8300-render.sys'='adf92be2b28f2b353eb0a247620cc23e8d9051841bea9aacbd2522ae08525d4b';'ge8300-mips.fw'='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'}
foreach($name in $fixed.Keys){Verify (Join-Path $BaselineDirectory $name) $fixed[$name]}
$inf=Join-Path $PSScriptRoot 'ge8300-render.inf';$text=[IO.File]::ReadAllText($inf)
if($text -notmatch '(?m)^DriverVer=11/07/2026,0\.0\.3\.14\s*$' -or $text -notmatch '(?m)^HKR,,UserModeDriverName,0x00010000,ge8300-umd.dll,ge8300-umd.dll,ge8300-umd.dll\s*$'){throw 'Committed 3.14 three-slot INF required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($name in @('ge8300-render.sys','ge8300-mips.fw','mt8167-development.cer')){Copy-Item (Join-Path $BaselineDirectory $name) $out}
 Copy-Item (Join-Path $NativeDirectory 'ge8300-umd.dll') $out;Copy-Item $inf $out
 Copy-Item $bp "$out\baseline-signed-manifest.json";Copy-Item $np "$out\native-signed-manifest.json"
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign="$kit\bin\x64\signtool.exe"
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out" /os:6_3_ARM
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 $store=@();if(Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"){$store+='/sm'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @store "$out\ge8300-render.cat"
 if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-render.inf')){
  & $sign verify /pa /c "$out\ge8300-render.cat" "$out\$name"
  if($LASTEXITCODE){throw 'CAT membership failed'}
 }
 foreach($name in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat')){
  $sig=Get-AuthenticodeSignature "$out\$name"
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Image/CAT signature invalid'}
 }
 foreach($name in $fixed.Keys){Verify "$out\$name" $fixed[$name]}
 @{package_source_commit=(& git -C $root rev-parse HEAD);binary_source_commit=$n.source_commit;kernel_binary_source_commit='9f31ddcf89fbe6d5c3a7b5abcceadb223212d378';baseline_manifest_sha256=$BaselineManifestSha256;native_manifest_sha256=$NativeManifestSha256;signer_thumbprint=$CertificateThumbprint;signature_status='Valid';inf_driverver='11/07/2026,0.0.3.14';kernel_changed=$false;firmware_changed=$false;limited_development_profile=$true;development_negotiation_pipeline='10_0';standard_profile_qualified=$false;runtime_verified=$false;outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-render.cat','mt8167-development.cer')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
