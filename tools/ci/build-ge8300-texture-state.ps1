# Source-only internal descriptor producer; no GPU caps/package/signing.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Explicit source-only descriptor build required'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel))
if(@(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
try{
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 $source="$root\drivers\gpu\mt8167\vendor_m1_11\render_owner\native_texture2d_state.c";$resource=(& clang -print-resource-dir)
 $args=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-mfpu=none','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-nostdinc','-isystem',"$resource\include",'-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type')
 & clang @args -c $source -o "$out\native_texture2d_state.obj";if($LASTEXITCODE){throw 'ARM descriptor TU compile'}
 $metadata=@(& llvm-readobj --file-headers "$out\native_texture2d_state.obj");if($LASTEXITCODE -or !($metadata -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'ARMNT required'}
 $metadata|Set-Content "$out\coff-metadata.txt"
 [ordered]@{schema='texture2d-source-state-arm-v1';source_commit=(& git -C $root rev-parse HEAD);compiled_units=@('native_texture2d_state');source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();compile_arguments=$args;SDK_rebuilt=$false;PCO_rebuilt=$false;existing_runtime_affected=$false;standard_texture_complete=$false;driver_linked=$false;signed=$false;hardware_verified=$false;object_sha256=(Get-FileHash "$out\native_texture2d_state.obj" -Algorithm SHA256).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
