# Opt-in module-only ARM32 build. Does not build/install a render SYS or UMD DLL.
param([switch]$EnableCandidate,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'WDDM render candidate is default off; pass -EnableCandidate explicitly'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -ne 0){throw 'Cannot resolve source checkout'}
$root=[IO.Path]::GetFullPath($root)
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-wddm-render'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside checkout'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Start-Transcript -Path (Join-Path $out 'build.log') -Append|Out-Null;$transcript=$true
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
 $resource=(& clang -print-resource-dir)
 if($LASTEXITCODE -ne 0 -or !(Test-Path (Join-Path $resource 'include\intrin.h'))){throw 'Missing compiler intrinsic headers'}
 $common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DDXGKDDI_INTERFACE_VERSION=0x4002','-DD3D_UMD_INTERFACE_VERSION=0x4002',"-I$inc\shared")
 foreach($unit in @('clear_protocol','clear_engine','capture','kmd_clear','kmd_objects','lifecycle','miniport','umd/clear_ddi','umd/bootstrap','testclient/d3d-clear','testclient/memory')) {
  $name=($unit -replace '/','_');$includes=@("-I$inc\um")
  if($unit -in @('capture','kmd_clear','kmd_objects','lifecycle','miniport')){$includes=@("-DGEO_LINKED_NATIVE_CORE=1","-D_KERNEL_MODE","-D__ldrexd=__geo_wdk81_ldrexd_declaration","-I$resource\include","-I$inc\km","-I$inc\km\crt")}
  & clang @common @includes -c (Join-Path $PSScriptRoot "$unit.c") -o (Join-Path $out "$name.obj")
  if($LASTEXITCODE -ne 0){throw "ARM32 compile failed: $unit"}
 }
 & llvm-lib /machine:arm "/out:$out\ge8300-kmd-clear.lib" "$out\clear_protocol.obj" "$out\clear_engine.obj" "$out\capture.obj" "$out\kmd_clear.obj" "$out\kmd_objects.obj" "$out\lifecycle.obj" "$out\miniport.obj"
 if($LASTEXITCODE -ne 0){throw 'KMD module archive failed'}
 & llvm-lib /machine:arm "/out:$out\ge8300-umd-clear.lib" "$out\clear_protocol.obj" "$out\umd_clear_ddi.obj" "$out\umd_bootstrap.obj" "$out\testclient_memory.obj"
 if($LASTEXITCODE -ne 0){throw 'UMD module archive failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\testclient_d3d-clear.obj" "$out\testclient_memory.obj" "$lib\kernel32.lib" "$lib\dxgi.lib" "$lib\d3d11.lib" "/out:$out\ge8300-d3d-clear.exe"
 if($LASTEXITCODE -ne 0){throw 'D3D client link failed'}
 & llvm-readobj --file-headers --coff-imports "$out\ge8300-d3d-clear.exe" | Set-Content "$out\pe-imports.txt"
 if($LASTEXITCODE -ne 0){throw 'PE inspection failed'}
 $commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -ne 0){throw 'Missing source revision'}
 $sources=@(Get-ChildItem $PSScriptRoot -File -Recurse | Where-Object {$_.Extension -in '.c','.h','.ps1','.md'} | ForEach-Object {@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 $outputs=@('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','ge8300-d3d-clear.exe')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 @{source_commit=$commit;source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);candidate_default_off=$true;wdk81_intrinsic_compat='__ldrexd declaration-only alias; no replacement implementation';kind='DDI implementation archives plus standard D3D client; no installable miniport/UMD';owner_abi='draft v1, not connected';runtime_verified=$false;sources=$sources;outputs=$outputs}|ConvertTo-Json -Depth 6|Set-Content "$out\build-manifest.json"
} finally {
 if($transcript){Stop-Transcript|Out-Null}
 $env:TEMP=$oldTemp;$env:TMP=$oldTmp
}
