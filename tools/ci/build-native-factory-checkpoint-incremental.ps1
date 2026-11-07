# SPDX-License-Identifier: MIT
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$BaselineLibrary,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit native initialization evidence opt-in required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if(@(& git -C $root status --porcelain).Count){throw 'Clean source required'}
$allowed=@('drivers/gpu/mt8167/vendor_m1_11/native_factory_checkpoint.h','drivers/gpu/mt8167/vendor_m1_11/native_owner_factory.c','drivers/gpu/mt8167/vendor_m1_11/native_owner_factory_platform.c','drivers/gpu/mt8167/vendor_m1_11/native_owner_allocation.c','tools/ci/build-native-factory-checkpoint-incremental.ps1')
foreach($p in @(& git -C $root diff --name-only e83792d88485883130e137309b0ef027b3c7bc5e HEAD)){if($p -notin $allowed){throw 'Unreviewed native source change'}}
if((Get-FileHash $BaselineLibrary -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'cee101f279430ff214e853d1104017941fb4df884e75d0e0a764c0eb22b54a87'){throw 'Native baseline archive mismatch'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar) -or (Test-Path $out)){throw 'Fresh source-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$resource=(& clang -print-resource-dir);$gpu="$root\drivers\gpu\mt8167"
$common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1','-D__ldrexd=__geo_wdk81_ldrexd_declaration','-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1','-DA04_GE8300_NATIVE_OWNER_DRAW=1',"-I$resource\include","-I$inc\km","-I$inc\km\crt","-I$inc\shared","-I$gpu","-I$gpu\fwif","-I$gpu\compat","-I$gpu\vendor_m1_11")
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$lib="$out\ge8300-native-owner-core.lib";Copy-Item $BaselineLibrary $lib
 $members=@(& llvm-ar t $lib);$objects=@()
 foreach($unit in @('native_owner_factory','native_owner_factory_platform','native_owner_allocation')){
  $member='vendor_m1_11-'+$unit+'.obj';if(@($members|Where-Object {[IO.Path]::GetFileName($_) -eq $member}).Count -ne 1){throw 'Expected one fixed native archive member'}
  $obj="$out\$member";& clang @common -c "$gpu\vendor_m1_11\$unit.c" -o $obj;if($LASTEXITCODE){throw 'Native checkpoint compile failed'}
  & llvm-ar r $lib $obj;if($LASTEXITCODE){throw 'Native member replacement failed'};$after=@(& llvm-ar t $lib);if($after.Count -ne $members.Count -or @($after|Where-Object {[IO.Path]::GetFileName($_) -eq $member}).Count -ne 1){throw 'Archive member replacement not unique'};$objects+=@{member=$member;sha256=(Get-FileHash $obj -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 @{source_commit=(& git -C $root rev-parse HEAD);baseline_source='e83792d88485883130e137309b0ef027b3c7bc5e';baseline_library_sha256='cee101f279430ff214e853d1104017941fb4df884e75d0e0a764c0eb22b54a87';outputs=@{name='ge8300-native-owner-core.lib';sha256=(Get-FileHash $lib -Algorithm SHA256).Hash.ToLowerInvariant()};objects=$objects;changed_abi=$false;default_off=$true;device_verified=$false}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\native-checkpoint-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
