param(
    [Parameter(Mandatory=$true)][string]$SourceWim,
    [Parameter(Mandatory=$true)][string]$OutputWim,
    [Parameter(Mandatory=$true)][string]$DriverPackage,
    [Parameter(Mandatory=$true)][string]$MountDirectory,
    [Parameter(Mandatory=$true)][string]$SourceSha256,
    [Parameter(Mandatory=$true)][string]$DriverSha256
)
$ErrorActionPreference='Stop'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $SourceWim).Hash -ne $SourceSha256) { throw 'Source WIM hash mismatch' }
if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $DriverPackage 'mt8167-toprgu.sys')).Hash -ne $DriverSha256) { throw 'TOPRGU driver hash mismatch' }
foreach ($name in @('mt8167-toprgu.inf','mt8167-toprgu.sys','mt8167-toprgu.cat')) {
    if (!(Test-Path -LiteralPath (Join-Path $DriverPackage $name) -PathType Leaf)) { throw "Missing package member: $name" }
}
if ((Test-Path -LiteralPath $OutputWim) -or (Test-Path -LiteralPath $MountDirectory)) { throw 'Output WIM or mount directory already exists' }
New-Item -ItemType Directory -Path $MountDirectory | Out-Null
Copy-Item -LiteralPath $SourceWim -Destination $OutputWim
$mounted=$false
try {
    & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory"
    if ($LASTEXITCODE -ne 0) { throw 'DISM WIM mount failed' }
    $mounted=$true
    & dism.exe "/Image:$MountDirectory" /Add-Driver "/Driver:$(Join-Path $DriverPackage 'mt8167-toprgu.inf')" /ForceUnsigned
    if ($LASTEXITCODE -ne 0) { throw 'DISM TOPRGU injection failed' }
    & dism.exe "/Image:$MountDirectory" /Get-Drivers /Format:Table
    if ($LASTEXITCODE -ne 0) { throw 'DISM driver inventory failed' }
    & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit
    if ($LASTEXITCODE -ne 0) { throw 'DISM WIM commit failed' }
    $mounted=$false
    Get-FileHash -Algorithm SHA256 -LiteralPath $OutputWim | Format-List
} finally {
    if ($mounted) { & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard }
}
