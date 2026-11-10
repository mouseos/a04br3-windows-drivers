# Source-only DWM candidate: preserve the strict recovered compiler375 cache.
param([Parameter(Mandatory=$true)][string]$VerifiedCompilerDirectory,
 [switch]$AllowAdapterDerivedCompilerCache,
 [Parameter(Mandatory=$true)][string]$ExpectedCompilerCacheManifestSha256,
 [Parameter(Mandatory=$true)][string]$VcToolsDirectory,
 [Parameter(Mandatory=$true)][string]$KernelVcToolsDirectory,
 [Parameter(Mandatory=$true)][string]$WindowsKit10Version,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
# These are process-local tool paths. No vcvars/machine/SDK/cache mutation.
$kernelCl=Join-Path $KernelVcToolsDirectory 'bin\Hostx64\arm\cl.exe'
if(!(Test-Path $kernelCl) -or (Get-Item $kernelCl).VersionInfo.FileVersion -notmatch '^19\.29\.'){
 throw 'This candidate requires the existing MSVC14.29 kernel compiler/linker'
}
if($ExpectedCompilerCacheManifestSha256 -notmatch '^[0-9A-Fa-f]{64}$' -or
 !(Test-Path (Join-Path $VerifiedCompilerDirectory 'cache-evidence.json'))){
 throw 'Recovered complete compiler375 strict cache and exact evidence manifest SHA required; no cold fallback'
}
# Shared internal layout changed: compile every native/KMD/UMD dependent TU
# from this checkout. No old driver objects are asserted to have include proof.
# The fullsource entry verifies the 375 sources/includes/args/tools/API before
# using the existing libraries, and performs no compiler or SDK reconstruction.
& "$PSScriptRoot\build-ge8300-from-source.ps1" -EnableCandidate -UnifiedDisplay `
 -VerifiedCompilerDirectory $VerifiedCompilerDirectory -AllowAdapterDerivedCompilerCache:$AllowAdapterDerivedCompilerCache `
 -ExpectedCompilerCacheManifestSha256 $ExpectedCompilerCacheManifestSha256 `
 -VcToolsDirectory $VcToolsDirectory -KernelVcToolsDirectory $KernelVcToolsDirectory `
 -WindowsKit10Version $WindowsKit10Version -OutputDirectory $OutputDirectory
if($LASTEXITCODE){throw 'DWM source build failed; preserve output/logs without signing or deployment'}
