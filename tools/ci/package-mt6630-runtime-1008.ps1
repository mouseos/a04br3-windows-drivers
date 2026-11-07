# Sign a newly built Native1008 SYS, then generate/sign a fresh catalog.
# No runner/device operation or certificate generation.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$BuildDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [ValidatePattern('^[a-fA-F0-9]{40}$')]
 [string]$CertificateThumbprint='BCBF6C456C6F0DD0818434763B189488FBF13570'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Inside([string]$Path){
 $full=[IO.Path]::GetFullPath($Path)
 if(!$full.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'All inputs/outputs must be inside this checkout'}
 return $full
}
$build=Inside $BuildDirectory;$out=Inside $OutputDirectory
if(Test-Path -LiteralPath $out){throw 'OutputDirectory must be new'}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Fixed source commit required'}
$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty){throw 'Use a clean committed source checkout'}
$m=Get-Content -Raw -LiteralPath (Join-Path $build 'manifest.json')|ConvertFrom-Json
if($m.source_commit -ne $commit -or $m.source_dirty -ne $false -or $m.wdk_link_succeeded -ne 1 -or $m.driver_version -ne '2026.11.6.1008'){throw 'Require the same fixed-source Native1008 build manifest'}
foreach($flag in @('A04_WIFI_SESSION_DIAGNOSTIC','A04_WIFI_IRQ_DIAGNOSTIC','A04_WIFI_CALIBRATION_DIAGNOSTIC')){
 if($m.critical_flags.$flag -ne 1){throw "Missing build flag: $flag"}
}
foreach($flag in @('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')){
 if($m.kernel_codegen_flags -notcontains $flag){throw "Missing ARM kernel codegen flag: $flag"}
}
if($m.kernel_runtime_libraries -notcontains 'libcntpr.lib'){throw 'WDK kernel runtime manifest required'}
$sys=Join-Path $build 'mt6630-runtime.sys';$inf=Join-Path $build 'mt6630-runtime.inf'
if((Hash $sys) -ne $m.sys_sha256 -or (Hash $inf) -ne $m.inf_sha256 -or (Hash $inf) -ne (Hash (Join-Path $root 'drivers\network\mt6630\mt6630-runtime.inf'))){throw 'Build input hashes disagree'}
if([IO.File]::ReadAllText($inf) -notmatch '(?m)^DriverVer=11/06/2026,2026\.11\.6\.1008\r?$'){throw 'Require version1008 INF'}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe';$inf2cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$cert=Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
if(!$cert.HasPrivateKey){throw 'Existing certificate private key required'}
$package=Join-Path $out 'package';$work=Join-Path $out 'work'
New-Item -ItemType Directory -Path $package,$work | Out-Null
Copy-Item -LiteralPath $sys,$inf -Destination $package
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
try {
 $env:TEMP=$work;$env:TMP=$work;$env:RUNNER_TEMP=$work
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint (Join-Path $package 'mt6630-runtime.sys')
 if($LASTEXITCODE){throw 'SYS signing failed'}
 & $inf2cat "/driver:$package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 & $sign sign /fd SHA256 /s My /sha1 $CertificateThumbprint (Join-Path $package 'mt6630-runtime.cat')
 if($LASTEXITCODE){throw 'CAT signing failed'}
 & $sign verify /pa /v (Join-Path $package 'mt6630-runtime.sys')
 if($LASTEXITCODE){throw 'SYS signature verification failed'}
 & $sign verify /pa /v /c (Join-Path $package 'mt6630-runtime.cat') (Join-Path $package 'mt6630-runtime.sys')
 if($LASTEXITCODE){throw 'Catalog membership verification failed'}
} finally {$env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner}
$files=@(Get-ChildItem -LiteralPath $package -File|Sort-Object Name|ForEach-Object{[ordered]@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Hash $_.FullName)}})
[ordered]@{
 status='signed-native1008-not-device-validated';source_commit=$commit;driver_ver=$m.driver_ver
 build_manifest_sha256=(Hash (Join-Path $build 'manifest.json'));build_manifest=$m
 unsigned_sys_sha256=$m.sys_sha256;expected_installed_sys_sha256=(Hash (Join-Path $package 'mt6630-runtime.sys'))
 certificate_thumbprint=$cert.Thumbprint;catalog_regenerated=$true;hardware_verified=$false;package_files=$files
} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
Write-Output "Native1008 package: $package. Compare installed DriverVer AND signed SYS hash."
