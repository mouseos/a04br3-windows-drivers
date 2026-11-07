# Reuse the exact signed brightness SYS; update the source INF and sign only its new CAT.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SignedSys,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$CatalogCertificateThumbprint,
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedSysSha256='71f4ddaec639045a8b7dc1aaa7bc4c341e3f5955e2bec3b00f7a1a91528c7dce',
    [string]$KitRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1"
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$inputSys=(Resolve-Path $SignedSys).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
$prefix=$root.TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar
if(-not $out.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this source checkout'}
if(Test-Path $out){throw 'OutputDirectory must not already exist'}
$inf=Join-Path $root 'drivers\gpu\mt8167\wddm-display\mt8167-display.inf'
$match=[regex]::Match((Get-Content -Raw $inf),'(?m)^DriverVer\s*=\s*([^,\r\n]+),\s*([0-9.]+)\s*$')
if(-not $match.Success -or ([version]$match.Groups[2].Value)-lt ([version]'0.0.1.4')){throw 'Fresh source INF version 0.0.1.4 or later is required; never reuse the candidate INF'}
$inputHash=(Get-FileHash $inputSys -Algorithm SHA256).Hash
if($inputHash -ne $ExpectedSysSha256){throw 'Signed SYS hash does not match the pinned brightness candidate'}
$signature=Get-AuthenticodeSignature $inputSys
if(-not $signature.SignerCertificate -or $signature.Status -in @('NotSigned','HashMismatch','NotSupportedFileFormat')){throw 'Input must retain its existing embedded signature'}
$inf2cat=Join-Path $KitRoot 'bin\x86\Inf2Cat.exe'
$signtool=Join-Path $KitRoot 'bin\x86\signtool.exe'
foreach($tool in @($inf2cat,$signtool)){if(-not (Test-Path $tool)){throw "Missing WDK tool: $tool"}}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try {
    $package=Join-Path $out 'package';$temporary=Join-Path $out 'tool-tmp'
    New-Item -ItemType Directory -Force $package,$temporary | Out-Null
    $env:TEMP=$temporary;$env:TMP=$temporary
    $sys=Join-Path $package 'mt8167-display.sys';$cat=Join-Path $package 'mt8167-display.cat'
    Copy-Item $inputSys $sys
    Copy-Item $inf (Join-Path $package 'mt8167-display.inf')
    if((Get-FileHash $sys -Algorithm SHA256).Hash -ne $inputHash){throw 'SYS copy changed bytes'}
    & $inf2cat "/driver:$package" /os:6_3_ARM
    if($LASTEXITCODE -ne 0){throw 'Inf2Cat failed; do not alter the clock or date to bypass it'}
    # Deliberately no SYS signing: a second embedded signature changes its pinned hash.
    & $signtool sign /fd SHA256 /sha1 $CatalogCertificateThumbprint /s My $cat
    if($LASTEXITCODE -ne 0){throw 'New CAT signing failed'}
    $catSignature=Get-AuthenticodeSignature $cat
    if(-not $catSignature.SignerCertificate -or $catSignature.SignerCertificate.Thumbprint -ne $CatalogCertificateThumbprint -or $catSignature.Status -in @('NotSigned','HashMismatch','NotSupportedFileFormat')){throw 'New CAT signature is missing or mismatched'}
    if((Get-FileHash $sys -Algorithm SHA256).Hash -ne $inputHash -or (Get-FileHash $inputSys -Algorithm SHA256).Hash -ne $inputHash){throw 'Pinned SYS bytes changed during catalog generation/signing'}
    @{
        status='new-INF-and-CAT-created-not-device-validated';binary_recompiled=$false;sys_resigned=$false;
        source_inf=$inf;driver_date=$match.Groups[1].Value;driver_version=$match.Groups[2].Value;
        expected_sys_sha256=$ExpectedSysSha256.ToLowerInvariant();input_signature_status=[string]$signature.Status;
        sys_sha256=(Get-FileHash $sys -Algorithm SHA256).Hash;inf_sha256=(Get-FileHash (Join-Path $package 'mt8167-display.inf') -Algorithm SHA256).Hash;
        cat_sha256=(Get-FileHash $cat -Algorithm SHA256).Hash;cat_signer_thumbprint=$catSignature.SignerCertificate.Thumbprint;
        cat_signature_status=[string]$catSignature.Status;target_certificate_trust='not-established-by-this-script';
        input_binary_provenance='caller-supplied signed brightness candidate, verified by exact SHA256';
        tools=@{inf2cat_sha256=(Get-FileHash $inf2cat -Algorithm SHA256).Hash;signtool_sha256=(Get-FileHash $signtool -Algorithm SHA256).Hash}
    } | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $out 'manifest.json')
    Write-Host "New display INF/CAT package: $package; original signed SYS preserved."
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
