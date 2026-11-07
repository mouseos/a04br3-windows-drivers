# Final SOURCE-LINKED candidate only; no signing, INF generation or deployment.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,
 [Parameter(Mandatory=$true)][string]$RenderContractLibrary,[Parameter(Mandatory=$true)][string]$FrontendLibrary,
 [Parameter(Mandatory=$true)][string]$NativeOwnerSha256,[Parameter(Mandatory=$true)][string]$RenderContractSha256,
 [Parameter(Mandatory=$true)][string]$FrontendSha256,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Linked render candidate is default off'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel))
if($LASTEXITCODE -ne 0){throw 'Missing source checkout'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-wddm-linked'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be inside checkout'}
$inputs=@(@{path=$NativeOwnerLibrary;hash=$NativeOwnerSha256;role='native owner core'},@{path=$RenderContractLibrary;hash=$RenderContractSha256;role='mandatory render DDI/caps'},@{path=$FrontendLibrary;hash=$FrontendSha256;role='real shader/caps UMD frontend'})
foreach($item in $inputs){
 $item.path=[IO.Path]::GetFullPath($item.path)
 if(!$item.path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $item.hash -notmatch '^[0-9a-fA-F]{64}$' -or !(Test-Path $item.path)){throw 'Missing fixed source-linked input inside checkout'}
 if((Get-FileHash $item.path -Algorithm SHA256).Hash -ne $item.hash){throw 'Source-linked input hash mismatch'}
}
& (Join-Path $PSScriptRoot 'build-wdk81.ps1') -EnableCandidate -OutputDirectory $out
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$km=Join-Path $kit 'Lib\winv6.3\km\arm';$um=Join-Path $kit 'Lib\winv6.3\um\arm'
$nativeLibrary=$inputs[0].path;$renderLibrary=$inputs[1].path;$frontendLibrary=$inputs[2].path
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
try {
 Start-Transcript -Path (Join-Path $out 'linked-build.log') -Append|Out-Null;$transcript=$true
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 -DGEO_LINKED_REAL_FRONTEND=1 -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $PSScriptRoot 'umd\entry.c') -o "$out\umd_entry.obj"
 if($LASTEXITCODE -ne 0){throw 'UMD export compilation failed'}
 foreach($item in $inputs){if((Get-FileHash $item.path -Algorithm SHA256).Hash -ne $item.hash){throw 'Link input changed during module build'}}
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\ge8300-kmd-clear.lib" $nativeLibrary $renderLibrary "$km\displib.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE -ne 0){throw 'Render SYS link failed: real owner and mandatory render contract must be implemented'}
 & lld-link /dll /noentry /machine:arm /nodefaultlib "$out\umd_entry.obj" "$out\ge8300-umd-clear.lib" $frontendLibrary "$um\kernel32.lib" "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE -ne 0){throw 'UMD link failed: real frontend must define geo_ge8300_frontend'}
 foreach($file in @('ge8300-render.sys','ge8300-umd.dll')) {
  & llvm-readobj --file-headers --coff-imports --coff-exports (Join-Path $out $file)|Set-Content (Join-Path $out "$file.pe.txt")
  if($LASTEXITCODE -ne 0){throw 'Linked PE inspection failed'}
 }
 @{source_commit=(& git -C $root rev-parse HEAD);source_dirty=(@(& git -C $root status --porcelain).Count -ne 0);inputs=$inputs;
  hardware='PowerVR GE8300 / ACPI MOU0006, exclusive replacement of KMDF baseline';display='MOU0007 DOD unchanged';
  default_off=$true;runtime_verified=$false;signed=$false;installed=$false;
  outputs=@('ge8300-render.sys','ge8300-umd.dll')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 6|Set-Content "$out\linked-manifest.json"
} finally {if($transcript){Stop-Transcript|Out-Null};$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
