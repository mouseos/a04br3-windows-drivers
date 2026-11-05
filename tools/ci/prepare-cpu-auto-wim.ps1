param(
 [Parameter(Mandatory=$true)][string]$SourceWim,
 [Parameter(Mandatory=$true)][string]$SourceSha256,
 [Parameter(Mandatory=$true)][string]$Reporter,
 [Parameter(Mandatory=$true)][string]$ReporterSha256,
 [Parameter(Mandatory=$true)][string]$Token,
 [Parameter(Mandatory=$true)][string]$OutputWim,
 [Parameter(Mandatory=$true)][string]$MountDirectory
)
$ErrorActionPreference='Stop'
if($Token -cnotmatch '^[0-9A-F]{16}$') {throw 'invalid report token'}
if((Get-FileHash -Algorithm SHA256 $SourceWim).Hash -ne $SourceSha256) {throw 'WIM mismatch'}
if((Get-FileHash -Algorithm SHA256 $Reporter).Hash -ne $ReporterSha256) {throw 'reporter mismatch'}
$sig=Get-AuthenticodeSignature $Reporter
if(!$sig.SignerCertificate -or $sig.SignerCertificate.Thumbprint -ne '5FC3D0043DE4A471F738FB923FEC87336E81D43C') {throw 'reporter signer mismatch'}
if((Test-Path $OutputWim) -or (Test-Path $MountDirectory)) {throw 'output or mount exists'}
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$template=Join-Path $root 'tools\winpe\startnet-cpu-audit.cmd.in'
$start=Get-Content -Raw $template
if(([regex]::Matches($start,'@TOKEN@')).Count -ne 1) {throw 'invalid template'}
$start=$start.Replace('@TOKEN@',$Token)
Copy-Item $SourceWim $OutputWim; New-Item -ItemType Directory -Path $MountDirectory | Out-Null
$mounted=$false
try {
 & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory" | Out-Host
 if($LASTEXITCODE){throw 'mount failed'};$mounted=$true
 $a=Join-Path $MountDirectory 'a04br3';$net=Join-Path $MountDirectory 'Windows\System32\startnet.cmd'
 foreach($name in @('audit-toprgu-installed.cmd','guard-windows-volume.exe','a04-msdc-probe.inf','toprgu-audit-candidate.sys')) {if(!(Test-Path (Join-Path $a $name))){throw "missing $name"}}
 if((Get-FileHash -Algorithm SHA256 $net).Hash -ne 'D41A6E341F1924F65512994CBD3E2F9317435D9AB79768B35BDF915C6EBE3DF9') {throw 'unexpected original startnet'}
 foreach($name in @('startnet-before-cpu-audit.cmd','uart-cpu-report.exe')) {if(Test-Path (Join-Path $a $name)){throw "existing $name"}}
 Copy-Item $net (Join-Path $a 'startnet-before-cpu-audit.cmd');Copy-Item $Reporter (Join-Path $a 'uart-cpu-report.exe')
 [IO.File]::WriteAllText($net,$start,[Text.Encoding]::ASCII)
 $startHash=(Get-FileHash -Algorithm SHA256 $net).Hash
 & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit | Out-Host
 if($LASTEXITCODE){throw 'commit failed'};$mounted=$false
} finally {if($mounted){& dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard | Out-Host}}
[ordered]@{source_commit=(git -C $root rev-parse HEAD); source_wim_sha256=$SourceSha256; reporter_sha256=$ReporterSha256; startnet_sha256=$startHash; original_startnet_sha256='D41A6E341F1924F65512994CBD3E2F9317435D9AB79768B35BDF915C6EBE3DF9'; token=$Token; output_wim_sha256=(Get-FileHash -Algorithm SHA256 $OutputWim).Hash; status='unattended read-only audit; device operation unverified; no reset or DISM'} | ConvertTo-Json | Set-Content -Encoding UTF8 "$OutputWim.json"
