param(
    [Parameter(Mandatory=$true)][string]$StorageSys,
    [Parameter(Mandatory=$true)][string]$TouchSys,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
if (Test-Path $Output) { throw 'Output already exists' }
$inputs = @(
    @{ Name='storage'; Sys=$StorageSys; SysName='a04msdcprobe.sys'; Hash='BEB9D96B15B16ED5313F74F387832762167D69E61C237536441ED25189A1D58A'; Inf='drivers\storage\mt8167-msdc\mt8167-msdc-legacy.inf'; Cat='mt8167-msdc-legacy.cat' },
    @{ Name='touch'; Sys=$TouchSys; SysName='a04touch.sys'; Hash='3C1D86FE403B7C754326140D5331E1BCE5A05AB95B6415F6381ECE162D20FA8A'; Inf='drivers\input\bu21025\bu21025-legacy.inf'; Cat='bu21025-legacy.cat' }
)
foreach ($item in $inputs) {
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $item.Sys).Hash -ne $item.Hash) {
        throw "Unexpected $($item.Name) binary; refusing to build"
    }
}
New-Item -ItemType Directory -Path $Output | Out-Null
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=MT8167 BU21025 Development' -CertStoreLocation 'Cert:\CurrentUser\My' -NotBefore (Get-Date).AddYears(-12) -NotAfter (Get-Date).AddYears(3)
Export-Certificate -Cert $cert -FilePath (Join-Path $Output 'legacy-names.cer') | Out-Null
foreach ($item in $inputs) {
    $dir = Join-Path $Output $item.Name
    New-Item -ItemType Directory -Path $dir | Out-Null
    Copy-Item -LiteralPath $item.Sys -Destination (Join-Path $dir $item.SysName)
    Copy-Item -LiteralPath (Join-Path $root $item.Inf) -Destination $dir
    & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$dir" /os:6_3_ARM /uselocaltime
    if ($LASTEXITCODE -ne 0) { throw "Catalog generation failed: $($item.Name)" }
    & (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $dir $item.Cat)
    if ($LASTEXITCODE -ne 0) { throw "Catalog signing failed: $($item.Name)" }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $dir $item.SysName)).Hash -ne $item.Hash) { throw 'Binary changed' }
}
Write-Host 'Catalogs are test-signed. Installed-system trust, servicing and PnP are not verified.'
