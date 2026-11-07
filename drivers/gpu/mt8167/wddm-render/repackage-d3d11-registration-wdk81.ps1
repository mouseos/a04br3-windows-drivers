# SPDX-License-Identifier: MIT
# INF/CAT-only D3D9/10/11 name-list correction. No compile or image re-signing.
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$BaselineManifestSha256,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570')
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit limited development opt-in required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
$commit=(& git -C $root rev-parse HEAD)
$allowed=@('drivers/gpu/mt8167/wddm-render/ge8300-render.inf','drivers/gpu/mt8167/wddm-render/POSTSTART.md','drivers/gpu/mt8167/wddm-render/repackage-d3d11-registration-wdk81.ps1')
foreach($p in @(& git -C $root diff --name-only 9f31ddcf89fbe6d5c3a7b5abcceadb223212d378 HEAD)){if($p -notin $allowed){throw 'Unexpected code change in INF-only update'}}
function Verify($path,$hash){if($hash -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash.ToLowerInvariant()){throw 'Fixed input hash mismatch'}}
$bm=Join-Path $BaselineDirectory 'signed-manifest.json';Verify $bm $BaselineManifestSha256
$b=Get-Content -Raw $bm|ConvertFrom-Json
if($b.package_source_commit -ne '9f31ddcf89fbe6d5c3a7b5abcceadb223212d378' -or $b.inf_driverver -ne '11/07/2026,0.0.3.12' -or $b.signature_status -ne 'Valid' -or $b.signer_thumbprint -ne $CertificateThumbprint){throw 'Signed 3.12 baseline required'}
foreach($f in $b.outputs){Verify (Join-Path $BaselineDirectory $f.name) $f.sha256}
$pins=@{'ge8300-render.sys'='adf92be2b28f2b353eb0a247620cc23e8d9051841bea9aacbd2522ae08525d4b';'ge8300-umd.dll'='f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b';'ge8300-mips.fw'='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'}
foreach($n in $pins.Keys){Verify (Join-Path $BaselineDirectory $n) $pins[$n]}
Verify (Join-Path $BaselineDirectory 'linked-manifest.json') $b.linked_manifest_sha256
$inf=Join-Path $PSScriptRoot 'ge8300-render.inf';$text=[IO.File]::ReadAllText($inf)
if($text -notmatch '(?m)^DriverVer=11/07/2026,0\.0\.3\.13\s*$' -or
   $text -notmatch '(?m)^HKR,,UserModeDriverName,0x00010000,ge8300-umd.dll,ge8300-umd.dll,ge8300-umd.dll\s*$' -or
   $text -notmatch '(?m)^HKR,,InstalledDisplayDrivers,0x00010000,ge8300-umd,ge8300-umd,ge8300-umd\s*$'){throw 'Exact committed three-slot D3D registration required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar) -or (Test-Path $out)){throw 'Fresh source-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','mt8167-development.cer','linked-manifest.json')){Copy-Item (Join-Path $BaselineDirectory $n) $out}
 Copy-Item $inf $out;Copy-Item $bm "$out\baseline-signed-manifest.json"
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$cat="$kit\bin\x86\Inf2Cat.exe";$sign="$kit\bin\x64\signtool.exe"
 & $cat "/driver:$out" /os:6_3_ARM;if($LASTEXITCODE){throw 'Inf2Cat failed'}
 $store=@();if(Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"){$store+='/sm'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @store "$out\ge8300-render.cat";if($LASTEXITCODE){throw 'CAT signing failed'}
 foreach($n in $pins.Keys){Verify "$out\$n" $pins[$n];& $sign verify /pa /c "$out\ge8300-render.cat" "$out\$n";if($LASTEXITCODE){throw 'CAT membership failed'}}
 foreach($n in @('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat')){$sig=Get-AuthenticodeSignature "$out\$n";if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Existing image or new CAT signature rejected'}}
 $b.package_source_commit=$commit;$b.inf_driverver='11/07/2026,0.0.3.13'
 $b|Add-Member -NotePropertyName inf_only_registration -NotePropertyValue @{baseline_source_commit='9f31ddcf89fbe6d5c3a7b5abcceadb223212d378';baseline_manifest_sha256=$BaselineManifestSha256;compiled=$false;resigned_images=$false;d3d_name_slots=3;source='Microsoft enabling-support-for-the-direct3d-version-11-ddi'}
 $b.outputs=@('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-render.cat','mt8167-development.cer')|ForEach-Object {@{name=$_;sha256=(Get-FileHash "$out\$_" -Algorithm SHA256).Hash.ToLowerInvariant()}}
 $b|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
