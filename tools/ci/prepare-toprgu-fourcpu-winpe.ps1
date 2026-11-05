param(
 [Parameter(Mandatory=$true)][string]$SourceWim,
 [Parameter(Mandatory=$true)][string]$OutputRoot,
 [Parameter(Mandatory=$true)][string]$SourceSha256
)
$ErrorActionPreference='Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $SourceWim).Hash -ne $SourceSha256) { throw 'Source WIM mismatch' }
if (Test-Path $OutputRoot) { throw 'Output already exists' }
New-Item -ItemType Directory $OutputRoot | Out-Null
$env:A04_TOPRGU_CPU1_HANDOFF='1'
$env:A04_TOPRGU_UART_SCREEN='1'
$env:A04_TOPRGU_THERMAL_STOP='0'
$env:A04_TOPRGU_DEADLINE_SECONDS='0'
& (Join-Path $PSScriptRoot 'build-mt8167-toprgu.ps1')
$package=Join-Path $OutputRoot 'signed'
& (Join-Path $PSScriptRoot 'sign-toprgu-early.ps1') -UnsignedPackage (Join-Path $env:RUNNER_TEMP 'mt8167-toprgu') -SignedPackage $package
# The reused build directory may contain an unsigned manifest from an earlier
# build with different flags. It must never describe this signed package.
$staleManifest=Join-Path $package 'manifest.json'
if (Test-Path $staleManifest) { Remove-Item -LiteralPath $staleManifest }
$sys=Join-Path $package 'mt8167-toprgu.sys'
$sysHash=(Get-FileHash -Algorithm SHA256 $sys).Hash
$wim=Join-Path $OutputRoot 'diagnostic.wim'
$mount=Join-Path $OutputRoot 'mount'
& (Join-Path $PSScriptRoot 'prepare-toprgu-early-wim.ps1') -SourceWim $SourceWim -OutputWim $wim -DriverPackage $package -MountDirectory $mount -SourceSha256 $SourceSha256 -SysSha256 $sysHash
# Independently remount and check the selected SYS, service and certificate.
$loaded=$false; $mounted=$false
try {
 & dism.exe /Mount-Image "/ImageFile:$wim" /Index:1 "/MountDir:$mount" /ReadOnly | Out-Host
 if($LASTEXITCODE) { throw 'Verification mount failed' }; $mounted=$true
 $selected=Join-Path $mount 'Windows\System32\drivers\mt8167-toprgu.sys'
 if (!(Test-Path $selected) -or (Get-FileHash -Algorithm SHA256 $selected).Hash -ne $sysHash) { throw 'Selected SYS differs from signed candidate' }
 $checkHive=Join-Path $OutputRoot 'verify-system.hive'
 Copy-Item -LiteralPath (Join-Path $mount 'Windows\System32\config\SYSTEM') -Destination $checkHive
 & reg.exe load HKLM\A04BR3CHECK $checkHive | Out-Host
 if($LASTEXITCODE) { throw 'SYSTEM verification load failed' }; $loaded=$true
 $current=(Get-ItemProperty 'HKLM:\A04BR3CHECK\Select').Current
 $svc="HKLM:\A04BR3CHECK\ControlSet$($current.ToString('000'))\Services\Mt8167Toprgu"
 $s=Get-ItemProperty $svc
 if ($s.Start -ne 0 -or $s.Group -ne 'System Bus Extender') { throw 'Selected service not boot start' }
 & reg.exe unload HKLM\A04BR3CHECK | Out-Host
 if($LASTEXITCODE) { throw 'SYSTEM verification unload failed' }; $loaded=$false
} finally {
 if($loaded) { & reg.exe unload HKLM\A04BR3CHECK | Out-Host }
 if($mounted) { & dism.exe /Unmount-Image "/MountDir:$mount" /Discard | Out-Host }
}
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $package 'mt8167-toprgu.cer'))
$files=@{}
foreach($name in @('mt8167-toprgu.sys','mt8167-toprgu.inf','mt8167-toprgu.cat','mt8167-toprgu.cer')) { $files[$name]=(Get-FileHash -Algorithm SHA256 (Join-Path $package $name)).Hash }
[ordered]@{ source_commit=(git -C $root rev-parse HEAD); source_wim_sha256=$SourceSha256; wim_sha256=(Get-FileHash -Algorithm SHA256 $wim).Hash; current_control_set=$current; sys_sha256=$sysHash; cert_thumbprint=$cert.Thumbprint; package_sha256=$files; cpu1_handoff=1; uart_screen=1; thermal_stop=0; deadline_seconds=0; status='test-signed diagnostic WIM, device trust and operation unverified' } | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $OutputRoot 'manifest.json')
