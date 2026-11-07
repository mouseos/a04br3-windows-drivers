# Compile-only checkpoint: sidecar/client plus fixed KMDF-free native modules.
# Does not create an installable SYS, run privateclear, sign or deploy anything.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$NativeSourceDirectory,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{40}$')][string]$NativeCommit,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Combined source candidate is default off'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel))
if($LASTEXITCODE -ne 0){throw 'Missing sidecar checkout'}
$native=[IO.Path]::GetFullPath($NativeSourceDirectory)
$actual=(& git -C $native rev-parse HEAD)
if($LASTEXITCODE -ne 0 -or $actual -ne $NativeCommit){throw 'Native source must be a fixed checkout at NativeCommit'}
if(@(& git -C $native status --porcelain --untracked-files=no).Count){throw 'Native tracked source is dirty'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-combined-modules'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside sidecar checkout'}
& (Join-Path $PSScriptRoot 'build-wdk81.ps1') -EnableCandidate -OutputDirectory $out
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$resource=(& clang -print-resource-dir)
if($LASTEXITCODE -ne 0){throw 'Cannot identify Clang intrinsic headers'}
$gpu=Join-Path $native 'drivers\gpu\mt8167'
$units=@('vendor_m1_11/native_owner_mdl','vendor_m1_11/native_owner_irq','vendor_m1_11/windows_native_submit','dma_pages')
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Start-Transcript -Path (Join-Path $out 'combined-build.log') -Append|Out-Null;$transcript=$true
 $objects=@();$sources=@()
 foreach($unit in $units){
  $source=Join-Path $gpu "$unit.c";$object=Join-Path $out (('native_'+($unit -replace '/','_'))+'.obj')
  & clang --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -D__ldrexd=__geo_wdk81_ldrexd_declaration "-I$resource\include" "-I$inc\km" "-I$inc\shared" "-I$inc\km\crt" "-I$gpu\compat" "-I$gpu\fwif" -c $source -o $object
  if($LASTEXITCODE -ne 0){throw "Native ARM32 compile failed: $unit"}
  $objects+=$object;$sources+=@{path="drivers/gpu/mt8167/$unit.c";sha256=(Get-FileHash $source -Algorithm SHA256).Hash}
 }
 & llvm-lib /machine:arm "/out:$out\ge8300-native-transport.lib" @objects
 if($LASTEXITCODE -ne 0){throw 'Native transport archive failed'}
 & llvm-readobj --file-headers --symbols "$out\ge8300-native-transport.lib" | Set-Content "$out\native-symbols.txt"
 if($LASTEXITCODE -ne 0){throw 'Native archive inspection failed'}
 if((& git -C $native rev-parse HEAD) -ne $NativeCommit -or @(& git -C $native status --porcelain --untracked-files=no).Count){throw 'Native source changed during build'}
 @{sidecar_commit=(& git -C $root rev-parse HEAD);native_commit=$NativeCommit;native_sources=$sources;
  kind='compile-only sidecar/client and KMDF-free native transport modules';factory_linked=$false;
  standard_d3d_connected=$false;runtime_verified=$false;installed=$false;
  outputs=@('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','ge8300-native-transport.lib','ge8300-d3d-clear.exe')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 6|Set-Content "$out\combined-manifest.json"
} finally {if($transcript){Stop-Transcript|Out-Null};$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
