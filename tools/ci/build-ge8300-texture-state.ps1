# Source-only internal descriptor producer; no GPU caps/package/signing.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$VcInclude,[string]$UcrtInclude,[string]$UmHeaderRoot)
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
 $nativeProgram="$root\drivers\gpu\mt8167\vendor_m1_11\render_owner\native_texture2d_program.c"
 & clang @args -c $nativeProgram -o "$out\native_texture2d_program.obj";if($LASTEXITCODE){throw 'Native sample admission TU compile'}

 # Compiler extension uses the existing PCO/NIR headers and port flags, but
 # does not rebuild or replace their libraries. No default compiler changes.
 $plan=Get-Content "$root\third_party\mesa-pco\source-plan.json" -Raw|ConvertFrom-Json
 $recipe=@($plan.units|Where-Object{$_.source -ceq 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c'})
 if($recipe.Count -ne 1 -or !$VcInclude -or !$UcrtInclude -or !$UmHeaderRoot){throw 'Explicit existing compiler header inputs required'}
 $compilerArgs=@();foreach($flag in $recipe[0].arguments){$s=[string]$flag;$s=$s.Replace('${REPO}',$root.Replace('\','/')).Replace('${VC_INCLUDE}',$VcInclude.Replace('\','/')).Replace('${UCRT_INCLUDE}',$UcrtInclude.Replace('\','/')).Replace('${SDK_SHARED}',($UmHeaderRoot+'\shared').Replace('\','/')).Replace('${SDK_UM}',($UmHeaderRoot+'\um').Replace('\','/'));if($s.Contains('${')){throw 'Unexpanded source plan variable'};$compilerArgs+=$s}
 $compilerOutputs=@()
 foreach($unit in @('texture2d_sm4','pco_texture2d')){
  $code="$root\drivers\gpu\mt8167\wddm-render\compiler\$unit.c"
  & clang @compilerArgs -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c $code -o "$out\$unit.obj"
  if($LASTEXITCODE){throw "Texture compiler extension TU failed: $unit"}
  $compilerOutputs+=@{unit=$unit;source_sha256=(Get-FileHash $code -Algorithm SHA256).Hash.ToLowerInvariant();object_sha256=(Get-FileHash "$out\$unit.obj" -Algorithm SHA256).Hash.ToLowerInvariant()}
 }

 [ordered]@{schema='texture2d-source-state-arm-v1';source_commit=(& git -C $root rev-parse HEAD);compiled_units=@('native_texture2d_state','native_texture2d_program');native_program_source_sha256=(Get-FileHash $nativeProgram -Algorithm SHA256).Hash.ToLowerInvariant();native_program_object_sha256=(Get-FileHash "$out\native_texture2d_program.obj" -Algorithm SHA256).Hash.ToLowerInvariant();compiler_extension_units=$compilerOutputs;compiler_arguments=$compilerArgs;source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant();compile_arguments=$args;SDK_rebuilt=$false;PCO_rebuilt=$false;existing_runtime_affected=$false;standard_texture_complete=$false;driver_linked=$false;signed=$false;hardware_verified=$false;object_sha256=(Get-FileHash "$out\native_texture2d_state.obj" -Algorithm SHA256).Hash.ToLowerInvariant()}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\build-manifest.json"
}finally{$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
