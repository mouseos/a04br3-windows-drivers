param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$InstalledDriverVer,
    [Parameter(Mandatory=$true)][string]$InstalledSysSha256
)
$ErrorActionPreference='Stop'
$packagePath=(Resolve-Path $Package).Path
$inf=Join-Path $packagePath 'mt8167-ge8300.inf'
$sys=Join-Path $packagePath 'mt8167-ge8300.sys'
$configPath=Join-Path $packagePath 'build-config.json'
$config=Get-Content $configPath -Raw | ConvertFrom-Json
$driverVer=[regex]::Match((Get-Content $inf -Raw),'(?im)^DriverVer\s*=\s*([^\r\n]+)').Groups[1].Value.Trim()
function VersionPair([string]$value) {
    $pair=$value.Split(',')
    if ($pair.Count -ne 2) { throw 'Invalid DriverVer' }
    return @{date=[datetime]::ParseExact($pair[0].Trim(),'MM/dd/yyyy',[cultureinfo]::InvariantCulture);version=[version]$pair[1].Trim()}
}
$new=VersionPair $driverVer
$old=VersionPair $InstalledDriverVer
if ($new.date -lt $old.date -or $new.version -le $old.version) { throw 'INF is not newer than selected installed package' }
if ($InstalledSysSha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid installed SYS hash' }
if ($config.driver_ver -ne $driverVer -or $config.installed_driver_ver -ne $InstalledDriverVer -or
    $config.installed_sys_sha256 -ne $InstalledSysSha256) { throw 'Package/config/target baseline mismatch' }
if ((Get-FileHash $inf -Algorithm SHA256).Hash -ne $config.inf_sha256) { throw 'INF changed after catalog preparation' }
$signature=Get-AuthenticodeSignature $sys
if ($signature.Status -ne 'Valid') { throw "Final signed SYS signature is not valid: $($signature.Status)" }
$files=@{}
foreach ($name in @('mt8167-ge8300.sys','mt8167-ge8300.inf','mt8167-ge8300.cat','ge8300-mips.fw')) {
    $path=Join-Path $packagePath $name
    if (-not (Test-Path $path -PathType Leaf)) { throw "Missing final package file $name" }
    $files[$name]=@{sha256=(Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant();bytes=(Get-Item $path).Length}
}
$catalogSignature=Get-AuthenticodeSignature (Join-Path $packagePath 'mt8167-ge8300.cat')
if ($catalogSignature.Status -ne 'Valid') { throw "Final catalog signature is not valid: $($catalogSignature.Status)" }
@{source_commit=$config.source_commit;driver_ver=$driverVer;files=$files;
  selected_installed_driver_ver=$InstalledDriverVer;selected_installed_sys_sha256=$InstalledSysSha256.ToLowerInvariant();
  native_clear_job=$config.native_clear_job;native_clear_activate=$config.native_clear_activate;native_clear_frames=$config.native_clear_frames;native_ta3d_publish_gate=$config.native_ta3d_publish_gate;
  signer_thumbprint=$signature.SignerCertificate.Thumbprint;signature_status=[string]$signature.Status;
  windows_native_applied=$false;windows_3d_success=$false;fingerprint_state='final-signed-package'} |
    ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $packagePath 'provider-package-manifest.json')
Write-Host "Final signed package hashes fixed, DriverVer=$driverVer. This does not prove target selection or 3D execution."
