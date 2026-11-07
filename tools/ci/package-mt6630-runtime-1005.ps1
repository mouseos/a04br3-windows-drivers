# INF-only repair: preserve the exact signed3ca98c9 checkpoint/PSP SYS.
# No compile, runner/device operation, parent rebuild or certificate creation.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$SignedSys,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [ValidatePattern('^[a-fA-F0-9]{40}$')]
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$expected='fdcc71a27b8b5df38f679934b34c1bd8dc567405ae0a6b878c6ae622b2612aba'
$binarySource='3ca98c90f10bd977fa79a32cbc54d7c799173899'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Hash $SignedSys) -ne $expected){throw 'Expected the exact signed3ca98c9 SYS; no stale SYS/recompile replacement'}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed source commit required'}
$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty){throw 'Use a clean committed source checkout'}
# This recipe repairs only packaging. Reject future C/header drift rather than
# silently labeling an older binary as a newly implemented driver.
$changed=(& git -C $root diff --name-only $binarySource HEAD -- drivers/network/mt6630)
if($LASTEXITCODE){throw 'Binary source unavailable for comparison'}
if(@($changed | Where-Object {$_ -match '\.(c|h)$'}).Count){throw 'C/header changes require a separate binary build/version'}
$inf=Join-Path $root 'drivers\network\mt6630\mt6630-runtime.inf'
$text=[IO.File]::ReadAllText($inf)
if($text -notmatch '(?m)^DriverVer=11/06/2026,2026\.11\.6\.1005\r?$'){throw 'Require INF1005'}
$section=[regex]::Match($text,'(?ms)^\[Install\.NT\]\s*\r?\n(?<body>.*?)(?=^\[|\z)').Groups['body'].Value
foreach($entry in @('*IfType=71','*MediaType=16','*PhysicalMediaType=9')){
 if($section -notmatch ('(?m)^'+[regex]::Escape($entry)+'\r?$')){throw "Missing DDInstall keyword: $entry"}
}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'OutputDirectory must be new and inside this checkout'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe';$inf2cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$cert.HasPrivateKey){throw 'Existing catalog signing private key required'}
$package=Join-Path $out 'package';$work=Join-Path $out 'work'
New-Item -ItemType Directory -Path $package,$work | Out-Null
Copy-Item -LiteralPath $SignedSys -Destination (Join-Path $package 'mt6630-runtime.sys')
Copy-Item -LiteralPath $inf -Destination (Join-Path $package 'mt6630-runtime.inf')
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
try {
 $env:TEMP=$work;$env:TMP=$work;$env:RUNNER_TEMP=$work
 & $sign verify /pa /v (Join-Path $package 'mt6630-runtime.sys')
 if($LASTEXITCODE){throw 'Existing signed SYS verification failed'}
 & $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Fresh Inf2Cat failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint (Join-Path $package 'mt6630-runtime.cat')
 if($LASTEXITCODE){throw 'CAT signing failed'}
 & $sign verify /pa /v /c (Join-Path $package 'mt6630-runtime.cat') (Join-Path $package 'mt6630-runtime.sys')
 if($LASTEXITCODE){throw 'CAT signature/membership verification failed'}
} finally {$env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner}
if((Hash (Join-Path $package 'mt6630-runtime.sys')) -ne $expected){throw 'Signed SYS changed during packaging'}
$files=@(Get-ChildItem -LiteralPath $package -File | Sort-Object Name | ForEach-Object{[ordered]@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Hash $_.FullName)}})
[ordered]@{
 status='inf1005-keywords-repackaged-not-device-validated';source_commit=$commit
 binary_source_commit=$binarySource;binary_rebuilt=$false;driver_ver='11/06/2026,2026.11.6.1005'
 expected_installed_sys_sha256=$expected;source_inf_sha256=(Hash $inf)
 fixed_ddinstall_keywords=@{'*IfType'=71;'*MediaType'=16;'*PhysicalMediaType'=9}
 catalog_regenerated=$true;certificate_thumbprint=$cert.Thumbprint
 hardware_verified=$false;package_files=$files
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
Write-Output "INF1005 keyword repair: $package. Same signed1004 SYS; new INF/CAT, device validation pending."
