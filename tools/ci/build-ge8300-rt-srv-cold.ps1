# 固定HEADのRT/SRV候補を一度cold buildし、compiler cacheを最初に回収可能にする。
param([switch]$EnableCandidate,
 [Parameter(Mandatory=$true)][string]$ExpectedSourceCommit,
 [string]$OutputDirectory,
 [string]$VcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207',
 [string]$KernelVcToolsDirectory='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133',
 [string]$WindowsKit10Version='10.0.22621.0',
 [ValidateRange(1,8)][int]$CompilerJobs=2)
$ErrorActionPreference='Stop'
$rtRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$VcToolsDirectory -or !$KernelVcToolsDirectory){throw 'Separate explicit PCO and kernel VC directories required for this cold entry'}
if(!$EnableCandidate -or $ExpectedSourceCommit -cnotmatch '^[0-9a-f]{40}$'){throw 'Explicit candidate and exact fixed source HEAD required'}
if((& git -C $rtRoot rev-parse HEAD) -cne $ExpectedSourceCommit -or @(& git -C $rtRoot status --porcelain).Count){throw 'Clean expected fixed source required'}
$rtOrigins=Get-Content "$PSScriptRoot/ge8300-fullsource-origins.json" -Raw|ConvertFrom-Json
if(!$rtOrigins.rt_srv_native_integration -or !$rtOrigins.rt_srv_native_integration.all_native_KMD_UMD_translation_units_recompile_required){throw 'RT/SRV full-source contract required'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $rtRoot 'build/rt-srv-cold'}
# VerifiedCompilerDirectory is deliberately not exposed here. A completed cache
# is preserved and later reused by the existing strict fullsource entry instead.
& "$PSScriptRoot/build-ge8300-from-source.ps1" -EnableCandidate -UnifiedDisplay -ArchiveCompilerCache -OutputDirectory $OutputDirectory -VcToolsDirectory $VcToolsDirectory -KernelVcToolsDirectory $KernelVcToolsDirectory -WindowsKit10Version $WindowsKit10Version -CompilerJobs $CompilerJobs
