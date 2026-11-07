# Opt-in module-only ARM32 build. Does not build/install a render SYS or UMD DLL.
param([switch]$EnableCandidate,[string]$OutputDirectory,[string]$ReuseDirectory,[string]$ReuseSourceCommit='5a26a0dd547561251f5e9fb188b4477d0699d791',[string]$MicrosoftArmCompiler,[string]$MicrosoftArmCompilerSha256,[string]$MicrosoftCrtInclude)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'WDDM render candidate is default off; pass -EnableCandidate explicitly'}
if(!$MicrosoftArmCompiler -or $MicrosoftArmCompilerSha256 -notmatch '^[0-9a-fA-F]{64}$' -or !(Test-Path $MicrosoftArmCompiler) -or (Get-FileHash $MicrosoftArmCompiler -Algorithm SHA256).Hash -ne $MicrosoftArmCompilerSha256){throw 'Real hash-pinned Microsoft ARM cl.exe is required for the independent MDL lock SEH TU'}
if(!(Test-Path (Join-Path $MicrosoftCrtInclude 'vcruntime.h'))){throw 'Fixed Microsoft compiler CRT headers required'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -ne 0){throw 'Cannot resolve source checkout'}
$root=[IO.Path]::GetFullPath($root)
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-wddm-render'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside checkout'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$reuse=$null;$reuseRecords=@();$reuseClient=$false;$reuseKmdHeaders=$false
function Same-ReuseSource([string]$relative) {
 if(!$reuse){return $false}
 $entry=@($reuse.sources|Where-Object {($_.path -replace '\\','/') -eq $relative})
 return $entry.Count -eq 1 -and (Get-FileHash (Join-Path $root $relative) -Algorithm SHA256).Hash -eq $entry[0].sha256
}
function Reuse-Output([string]$name) {
 $entry=@($reuse.outputs|Where-Object {$_.name -eq $name})
 $file=Join-Path $ReuseDirectory $name
 if($entry.Count -ne 1 -or !(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -ne $entry[0].sha256){throw "Reuse artifact hash mismatch: $name"}
 return $file
}
if($ReuseDirectory){
 $ReuseDirectory=[IO.Path]::GetFullPath($ReuseDirectory)
 if($ReuseDirectory -eq $out -or $ReuseSourceCommit -notmatch '^[0-9a-f]{40}$'){throw 'Use a distinct fixed reuse directory and commit'}
 $reuse=Get-Content -Raw (Join-Path $ReuseDirectory 'build-manifest.json')|ConvertFrom-Json
 if($reuse.source_commit -ne $ReuseSourceCommit -or $reuse.source_dirty){throw 'Reuse source provenance is not fixed/clean'}
 $reuseKmd=Reuse-Output 'ge8300-kmd-clear.lib';$reuseUmd=Reuse-Output 'ge8300-umd-clear.lib'
 $reuseKmdHeaders=$true
 foreach($header in Get-ChildItem $PSScriptRoot -Filter '*.h' -File){
  if(!(Same-ReuseSource ('drivers/gpu/mt8167/wddm-render/'+$header.Name))){$reuseKmdHeaders=$false}
 }
 if((Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/testclient/d3d-clear.c') -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/testclient/memory.c')){
  $reuseExe=Reuse-Output 'ge8300-d3d-clear.exe';$reuseClient=$true
 }
}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Start-Transcript -Path (Join-Path $out 'build.log') -Append|Out-Null;$transcript=$true
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
 $resource=(& clang -print-resource-dir)
 if($LASTEXITCODE -ne 0 -or !(Test-Path (Join-Path $resource 'include\intrin.h'))){throw 'Missing compiler intrinsic headers'}
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DDXGKDDI_INTERFACE_VERSION=0x4002','-DD3D_UMD_INTERFACE_VERSION=0x4002',"-I$inc\shared")
 foreach($unit in @('clear_protocol','frontend/draw_protocol','clear_engine','capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','umd/entry','testclient/d3d-clear','testclient/d3d9-flat','testclient/memory')) {
  $name=($unit -replace '/','_');$relative="drivers/gpu/mt8167/wddm-render/$unit.c"
  if($reuseClient -and $unit -eq 'testclient/d3d-clear'){$reuseRecords+=@{unit=$unit;kind='verified existing EXE';origin=$ReuseSourceCommit};continue}
  $archive=$null;$object=$name+'.obj';$reuseUnit=$false
  if($reuse -and (Same-ReuseSource $relative)){
   if($reuseKmdHeaders -and $unit -in @('clear_engine','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport')){$archive=$reuseKmd;$reuseUnit=$true}
   if($unit -eq 'clear_protocol' -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/clear_protocol.h')){$archive=$reuseKmd;$reuseUnit=$true}
   if($unit -eq 'frontend/draw_protocol' -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/frontend/draw_abi.h') -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/clear_protocol.h')){$archive=$reuseKmd;$reuseUnit=$true}
   if($unit -eq 'capture' -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/capture.h') -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/clear_protocol.h')){$archive=$reuseKmd;$reuseUnit=$true}
   if($unit -eq 'testclient/memory'){$archive=$reuseUmd;$reuseUnit=$true}
   # entry.c forwards two opaque pointers only. bootstrap.h and export ABI are
   # unchanged; it does not inspect the expanded device-private structure.
   if($unit -eq 'umd/entry' -and (Same-ReuseSource 'drivers/gpu/mt8167/wddm-render/umd/bootstrap.h')){
    $entryObject=Reuse-Output 'umd_entry.obj';Copy-Item $entryObject (Join-Path $out $object)
    $reuseRecords+=@{unit=$unit;kind='verified opaque export object';origin=$ReuseSourceCommit};continue
   }
  }
  if($reuseUnit){
   & llvm-ar x "--output=$out" $archive $object
   if($LASTEXITCODE -ne 0 -or !(Test-Path (Join-Path $out $object))){throw "Verified archive extraction failed: $object"}
   $reuseRecords+=@{unit=$unit;kind='member of hash-verified archive';origin=$ReuseSourceCommit};continue
  }
  $includes=@("-I$inc\um")
  if($unit -eq 'umd/entry'){$includes+='-DGEO_LINKED_REAL_FRONTEND=1'}
  if($unit -in @('umd/tokenized_shader','testclient/d3d-clear')){$includes+="-I$inc\km"}
  if($unit -in @('capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory')){$includes=@("-DGEO_LINKED_NATIVE_CORE=1","-D_KERNEL_MODE","-D__ldrexd=__geo_wdk81_ldrexd_declaration","-I$resource\include","-I$inc\km","-I$inc\km\crt")}
  $unitFlags=$common
  if($unit -eq 'umd/frontend'){$unitFlags=@($common|Where-Object {$_ -ne '-mfpu=none'})}
  & clang @unitFlags @includes -c (Join-Path $PSScriptRoot "$unit.c") -o (Join-Path $out "$name.obj")
  if($LASTEXITCODE -ne 0){throw "ARM32 compile failed: $unit"}
 }
 & $MicrosoftArmCompiler /nologo /c /O2 /GS- /kernel /Zl /TC /D_ARM_ /DARM /D_KERNEL_MODE /D_WIN32_WINNT=0x0603 /DNTDDI_VERSION=0x06030000 "/I$inc\shared" "/I$inc\km" "/I$inc\km\crt" "/I$MicrosoftCrtInclude" (Join-Path $PSScriptRoot 'mdl_pin.c') "/Fo$out\mdl_pin.obj"
 if($LASTEXITCODE -ne 0){throw 'Microsoft ARM SEH pin compile failed; do not replace with unsafe dereference'}
 $pinPe=@(& llvm-readobj --file-headers (Join-Path $out 'mdl_pin.obj'))
 if($LASTEXITCODE -ne 0 -or !($pinPe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'SEH compiler did not emit ARMNT'}
 & llvm-lib /machine:arm "/out:$out\ge8300-kmd-clear.lib" "$out\clear_protocol.obj" "$out\frontend_draw_protocol.obj" "$out\clear_engine.obj" "$out\capture.obj" "$out\kmd_clear.obj" "$out\kmd_objects.obj" "$out\query_adapter.obj" "$out\preempt.obj" "$out\render_contract.obj" "$out\lifecycle.obj" "$out\miniport.obj" "$out\frontend_memory.obj" "$out\mdl_pin.obj"
 if($LASTEXITCODE -ne 0){throw 'KMD module archive failed'}
 & llvm-lib /machine:arm "/out:$out\ge8300-umd-clear.lib" "$out\clear_protocol.obj" "$out\frontend_draw_protocol.obj" "$out\umd_clear_ddi.obj" "$out\umd_bootstrap.obj" "$out\umd_frontend.obj" "$out\umd_shader_state.obj" "$out\umd_tokenized_shader.obj" "$out\compiler_sm2.obj" "$out\testclient_memory.obj"
 if($LASTEXITCODE -ne 0){throw 'UMD module archive failed'}
 if($reuseClient){Copy-Item $reuseExe "$out\ge8300-d3d-clear.exe"}else{
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d-clear.obj" "$out\testclient_memory.obj" "$lib\kernel32.lib" "$lib\dxgi.lib" "$lib\d3d11.lib" "$lib\d3d9.lib" "/out:$out\ge8300-d3d-clear.exe"
 if($LASTEXITCODE -ne 0){throw 'D3D client link failed'}
 }
 & llvm-readobj --file-headers --coff-imports "$out\ge8300-d3d-clear.exe" | Set-Content "$out\pe-imports.txt"
 if($LASTEXITCODE -ne 0){throw 'PE inspection failed'}
 $commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -ne 0){throw 'Missing source revision'}
 & lld-link /machine:arm /subsystem:console /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d9-flat.obj" "$out\testclient_memory.obj" "$lib\d3d9.lib" "$lib\kernel32.lib" "$lib\user32.lib" "/out:$out\ge8300-d3d9-flat.exe"
 if($LASTEXITCODE -ne 0){throw 'Standard D3D9 flat client link failed'}
 $flatPe=@(& llvm-readobj --file-headers --coff-imports "$out\ge8300-d3d9-flat.exe")
 if($LASTEXITCODE -ne 0 -or !($flatPe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'D3D9 client is not ARMNT'}
 $flatPe|Set-Content "$out\ge8300-d3d9-flat.exe.pe.txt"
 $sources=@(Get-ChildItem $PSScriptRoot -File -Recurse | Where-Object {$_.Extension -in '.c','.h','.ps1','.md'} | ForEach-Object {@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 $outputs=@('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj','ge8300-d3d-clear.exe','ge8300-d3d9-flat.exe')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 @{source_commit=$commit;microsoft_seh_compiler_sha256=$MicrosoftArmCompilerSha256;microsoft_compile_units=@('mdl_pin');source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);candidate_default_off=$true;wdk81_intrinsic_compat='__ldrexd declaration-only alias; no replacement implementation';kind='DDI implementation archives plus standard D3D client; no installable miniport/UMD';owner_abi='in-process control-v2; native factory and full caps/frontend not linked';shader_compiler='real D3D9 TL/flat frontend and bounded shader/state capture; PCO/NIR ARM32 static closure linked separately';runtime_verified=$false;reuse_records=$reuseRecords;compile_units=@('clear_protocol','frontend/draw_protocol','clear_engine','capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','umd/entry','testclient/d3d-clear','testclient/d3d9-flat','testclient/memory');sources=$sources;outputs=$outputs}|ConvertTo-Json -Depth 6|Set-Content "$out\build-manifest.json"
} finally {
 if($transcript){Stop-Transcript|Out-Null}
 $env:TEMP=$oldTemp;$env:TMP=$oldTmp
}
