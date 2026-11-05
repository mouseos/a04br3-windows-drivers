param(
 [Parameter(Mandatory=$true)][string]$SourceWim,
 [Parameter(Mandatory=$true)][string]$SourceSha256,
 [Parameter(Mandatory=$true)][string]$CandidateSys,
 [Parameter(Mandatory=$true)][string]$CandidateSha256,
 [Parameter(Mandatory=$true)][string]$Certificate,
 [Parameter(Mandatory=$true)][string]$OutputWim,
 [Parameter(Mandatory=$true)][string]$MountDirectory
)
$ErrorActionPreference='Stop'
if ((Get-FileHash -Algorithm SHA256 $SourceWim).Hash -ne $SourceSha256) {throw 'WIM mismatch'}
if ((Get-FileHash -Algorithm SHA256 $CandidateSys).Hash -ne $CandidateSha256) {throw 'candidate SYS mismatch'}
$thumb=(New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($Certificate)).Thumbprint
if ($thumb -ne '5FC3D0043DE4A471F738FB923FEC87336E81D43C') {throw 'unexpected test certificate'}
if ((Test-Path $OutputWim) -or (Test-Path $MountDirectory)) {throw 'output or mount exists'}
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$script=Join-Path $root 'tools\winpe\audit-toprgu-installed.cmd'
Copy-Item $SourceWim $OutputWim
New-Item -ItemType Directory -Path $MountDirectory | Out-Null
$mounted=$false
try {
 & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory" | Out-Host
 if ($LASTEXITCODE) {throw 'mount failed'}; $mounted=$true
 $a=Join-Path $MountDirectory 'a04br3'
 foreach($name in @('guard-windows-volume.exe','compare-files.exe','a04-msdc-probe.inf')) {if (!(Test-Path (Join-Path $a $name))) {throw "missing $name"}}
 foreach($name in @('audit-toprgu-installed.cmd','toprgu-audit-candidate.sys')) {if (Test-Path (Join-Path $a $name)) {throw "existing $name"}}
 Copy-Item $script (Join-Path $a 'audit-toprgu-installed.cmd')
 Copy-Item $CandidateSys (Join-Path $a 'toprgu-audit-candidate.sys')
 if ((Get-FileHash -Algorithm SHA256 (Join-Path $a 'toprgu-audit-candidate.sys')).Hash -ne $CandidateSha256) {throw 'mounted SYS mismatch'}
 & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit | Out-Host
 if ($LASTEXITCODE) {throw 'commit failed'}; $mounted=$false
} finally { if ($mounted) { & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard | Out-Host } }
[ordered]@{source_commit=(git -C $root rev-parse HEAD); source_wim_sha256=$SourceSha256; candidate_sys_sha256=$CandidateSha256; cert_thumbprint=$thumb; audit_script_sha256=(Get-FileHash -Algorithm SHA256 $script).Hash; output_wim_sha256=(Get-FileHash -Algorithm SHA256 $OutputWim).Hash; status='audit WIM unvalidated on target; no disk servicing'} | ConvertTo-Json | Set-Content -Encoding UTF8 "$OutputWim.json"
