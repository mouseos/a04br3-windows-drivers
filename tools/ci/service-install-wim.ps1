param(
    [Parameter(Mandatory=$true)][string]$SourceWim,
    [Parameter(Mandatory=$true)][string]$OutputWim,
    [Parameter(Mandatory=$true)][string]$DriverPackage,
    [Parameter(Mandatory=$true)][string]$MountDirectory,
    [Parameter(Mandatory=$true)][string]$SourceSha256
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $SourceWim).Hash -ne $SourceSha256) { throw 'Source WIM hash mismatch' }
if (Test-Path -LiteralPath $OutputWim) { throw 'Output WIM already exists' }
if (Test-Path -LiteralPath $MountDirectory) { throw 'Mount directory already exists' }
foreach ($name in @('a04-msdc-probe.inf','a04msdcprobe.sys','a04msdcprobe.cat')) {
    if (!(Test-Path -LiteralPath (Join-Path $DriverPackage $name) -PathType Leaf)) { throw "Missing package member: $name" }
}
New-Item -ItemType Directory -Path $MountDirectory | Out-Null
Copy-Item -LiteralPath $SourceWim -Destination $OutputWim
$mounted = $false
try {
    & dism.exe /Mount-Image "/ImageFile:$OutputWim" /Index:1 "/MountDir:$MountDirectory"
    if ($LASTEXITCODE -ne 0) { throw 'DISM WIM mount failed' }
    $mounted = $true
    & dism.exe "/Image:$MountDirectory" /Add-Driver "/Driver:$(Join-Path $DriverPackage 'a04-msdc-probe.inf')"
    if ($LASTEXITCODE -ne 0) { throw 'DISM driver injection failed' }
    & dism.exe "/Image:$MountDirectory" /Get-Drivers /Format:Table
    if ($LASTEXITCODE -ne 0) { throw 'DISM driver inventory failed' }
    & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Commit
    if ($LASTEXITCODE -ne 0) { throw 'DISM WIM commit failed' }
    $mounted = $false
    Get-FileHash -Algorithm SHA256 -LiteralPath $OutputWim | Format-List
} finally {
    if ($mounted) { & dism.exe /Unmount-Image "/MountDir:$MountDirectory" /Discard }
}
