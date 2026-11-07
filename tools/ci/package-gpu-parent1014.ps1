# Fixed existing SD-on/SPM parent binary packaging only. No device operation.
param(
 [Parameter(Mandatory=$true)][string]$ExpectedRecipeCommit,
 [Parameter(Mandatory=$true)][string]$UnsignedDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570'
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$head=(& git -C $root rev-parse HEAD)
if($head -ne $ExpectedRecipeCommit -or @(& git -C $root status --porcelain).Count){throw 'Require clean fixed recipe commit'}
$source='287241c37fa0eae538e5e2d136dea5df42de7e43'
& git -C $root merge-base --is-ancestor $source $head
if($LASTEXITCODE){throw 'Missing fixed parent1014 SD-on ancestry'}
$changed=@(& git -C $root diff --name-only $source HEAD -- drivers/bus/mt8167-shared)
if($LASTEXITCODE -or $changed.Count){throw 'Parent hardware/profile source changed; no prior binary reuse'}
$in=[IO.Path]::GetFullPath($UnsignedDirectory)
$m=Get-Content (Join-Path $in 'gpu-parent1014-manifest.json') -Raw|ConvertFrom-Json
$p=Get-Content (Join-Path $in 'parent-manifest.json') -Raw|ConvertFrom-Json
if($m.source_commit -ne $source -or $p.source_commit -ne $source -or $m.profile -ne 'SPM_ASSIGNED_DOMAIN' -or !$m.spm_assigned -or $m.signed){throw 'Wrong fixed unsigned source/profile'}
if($m.parent_driver_ver -ne '11/07/2026,2026.11.7.1014'){throw 'Parent1014 INF identity required'}
foreach($entry in $m.outputs){
 if($entry.path -notin @('mt8167-shared.sys','shared-parent.inf','parent-manifest.json')){throw 'Unexpected input manifest path'}
 if((Get-FileHash (Join-Path $in $entry.path) -Algorithm SHA256).Hash -ne $entry.sha256){throw 'Unsigned parent output hash mismatch'}
}
$sys=Join-Path $in 'mt8167-shared.sys'
if((Get-FileHash $sys -Algorithm SHA256).Hash -ne '5C69937E2FEE4F26E7D2A3BE4806B37998187F4D594BC21E74C8A0E43A1C0A97'){throw 'Fixed raw binary mismatch'}
$profilePath=Join-Path $root 'drivers\bus\mt8167-shared\prototype\gpu-parent1014-spm-profile.json'
if((Get-FileHash $profilePath -Algorithm SHA256).Hash -ne $m.profile_sha256){throw 'Fixed flag profile mismatch'}
$profile=Get-Content $profilePath -Raw|ConvertFrom-Json
foreach($flag in $profile.flags.PSObject.Properties){if([string]$p.flags.($flag.Name) -ne [string]$flag.Value){throw "Effective flag mismatch: $($flag.Name)"}}
if($p.flags.A04_SHARED_SD_POWER -ne '1' -or $p.flags.A04_SHARED_GPU_DOMAIN_SPM -ne '1'){throw 'Missing SD/SPM ownership'}
$inf=Join-Path $root 'drivers\bus\mt8167-shared\prototype\shared-parent-engine-pcm-gpu-sd.inf'
if((Get-FileHash $inf -Algorithm SHA256).Hash -ne (Get-FileHash (Join-Path $in 'shared-parent.inf') -Algorithm SHA256).Hash){throw 'Input INF differs from fixed source'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Use a fresh output inside checkout'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x64\signtool.exe';$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$machine=Test-Path "Cert:\LocalMachine\My\$CertificateThumbprint"
$certPath=if($machine){"Cert:\LocalMachine\My\$CertificateThumbprint"}else{"Cert:\CurrentUser\My\$CertificateThumbprint"}
$cert=Get-Item $certPath;if(!$cert.HasPrivateKey){throw 'Existing signing identity unavailable'}
$storeFlags=@();if($machine){$storeFlags+='/sm'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Copy-Item $sys $out;Copy-Item $inf (Join-Path $out 'shared-parent.inf')
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @storeFlags (Join-Path $out 'mt8167-shared.sys')
 if($LASTEXITCODE){throw 'Parent SYS signing failed'}
 & $cat "/driver:$out" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Parent1014 Inf2Cat failed'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My @storeFlags (Join-Path $out 'shared-parent.cat')
 if($LASTEXITCODE){throw 'Parent CAT signing failed'}
 foreach($name in @('mt8167-shared.sys','shared-parent.cat')){
  $s=Get-AuthenticodeSignature (Join-Path $out $name)
  if($s.Status -ne 'Valid' -or $s.SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw "Invalid parent signature: $name"}
 }
 foreach($name in @('mt8167-shared.sys','shared-parent.inf')){
  & $sign verify /pa /c (Join-Path $out 'shared-parent.cat') (Join-Path $out $name)
  if($LASTEXITCODE){throw "Parent CAT membership failure: $name"}
 }
 Export-Certificate -Cert $cert -FilePath (Join-Path $out 'mt8167-development.cer')|Out-Null
 Copy-Item (Join-Path $in 'parent-manifest.json') (Join-Path $out 'unsigned-parent-manifest.json')
 Copy-Item (Join-Path $in 'gpu-parent1014-manifest.json') (Join-Path $out 'unsigned-gpu-manifest.json')
 @{source_commit=$source;package_recipe_commit=$head;driver_ver=$m.parent_driver_ver;profile='SPM_ASSIGNED_DOMAIN+SD1';effective_flags=$p.flags;
   unsigned_sys_sha256=(Get-FileHash $sys -Algorithm SHA256).Hash;signature_status='Valid';signer_thumbprint=$CertificateThumbprint;cat_membership_verified=$true;
   required_acpi_commit='6e852b1ff13e16609957afc7a2e03da3d371dfe8';required_boot_sha256='6dca720a68c8f62667ffec70669eb1326a0ca0145a6088313b96bcf89f2f93b4';installed=$false;runtime_verified=$false;
   outputs=@('mt8167-shared.sys','shared-parent.inf','shared-parent.cat','mt8167-development.cer')|ForEach-Object{@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}}|
   ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'signed-manifest.json')
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
