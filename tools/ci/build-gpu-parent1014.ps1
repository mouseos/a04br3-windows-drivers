# Fixed ecaa1008 matched parent profile + default-off GPU platform module.
param([switch]$EnableCandidate,[string]$ExpectedSourceCommit,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate -or !$ExpectedSourceCommit){throw 'Explicit candidate flag and fixed commit required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$head=(& git -C $root rev-parse HEAD);$expected=(& git -C $root rev-parse --verify "$($ExpectedSourceCommit)^{commit}")
if($LASTEXITCODE -or $head -ne $expected -or @(& git -C $root status --porcelain).Count){throw 'Require clean fixed source'}
& git -C $root merge-base --is-ancestor ecaa7b70248230dee2977e1d9057260f400287be $head
if($LASTEXITCODE){throw 'Must retain ecaa1008 ancestry/AFE bus forwarding'}
$profilePath=Join-Path $root 'drivers\bus\mt8167-shared\prototype\gpu-parent1014-profile.json'
$profile=Get-Content $profilePath -Raw|ConvertFrom-Json
$inf=Join-Path $root 'drivers\bus\mt8167-shared\prototype\shared-parent-engine-pcm.inf'
if((Get-Content $inf -Raw) -notmatch 'DriverVer=11/06/2026,2026\.11\.7\.1014'){throw 'Parent1014 package identity required'}
if(!$OutputDirectory){throw 'Dedicated OutputDirectory required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Output must be new and inside checkout'}
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
$saved=@{};$names=@('TEMP','TMP','RUNNER_TEMP')+@($profile.flags.PSObject.Properties.Name)
foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 foreach($property in $profile.flags.PSObject.Properties){[Environment]::SetEnvironmentVariable($property.Name,[string]$property.Value,'Process')}
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$out
 & (Join-Path $PSScriptRoot 'build-msdc-parent-probe.ps1')
 if($LASTEXITCODE){throw 'Matched parent ARM compile/link failed'}
 $built=Join-Path $out 'a04-msdc-parent-probe';$m=Get-Content (Join-Path $built 'parent-manifest.json') -Raw|ConvertFrom-Json
 foreach($property in $profile.flags.PSObject.Properties){if($m.flags.($property.Name) -ne $property.Value){throw "Profile mismatch: $($property.Name)"}}
 Copy-Item $inf (Join-Path $built 'shared-parent.inf')
 @{source_commit=$head;baseline_commit=$profile.source_commit;baseline_parent_ver=$profile.driver_ver;
   parent_driver_ver=$profile.target_driver_ver;profile='INFRA_ONLY';spm_assigned=$false;gpu_core_mapped=$false;
   dma_stop_proof_published=$false;geo_caps_published=$false;device_tested=$false;signed=$false;
   profile_sha256=(Get-FileHash $profilePath -Algorithm SHA256).Hash;
   outputs=@('mt8167-shared.sys','shared-parent.inf','parent-manifest.json'|ForEach-Object{$p=Join-Path $built $_;@{path=$_;sha256=(Get-FileHash $p -Algorithm SHA256).Hash}})}|
  ConvertTo-Json -Depth 6|Set-Content (Join-Path $built 'gpu-parent1014-manifest.json')
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}
