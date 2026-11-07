# Link-only fixed candidate: no repeated module compile, signing or deployment.
param([switch]$EnableCandidate,[Parameter(Mandatory=$true)][string]$ModuleDirectory,
 [Parameter(Mandatory=$true)][string]$NativeOwnerLibrary,[Parameter(Mandatory=$true)][string]$FrontendLibrary,
 [Parameter(Mandatory=$true)][string]$NativeOwnerSha256,[Parameter(Mandatory=$true)][string]$FrontendSha256,
 [Parameter(Mandatory=$true)][string]$CompilerLinkManifest,
 [Parameter(Mandatory=$true)][string]$CompilerLinkManifestSha256,[string]$CompilerLibraryDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Linked render candidate is default off'}
$root=[IO.Path]::GetFullPath((& git -C $PSScriptRoot rev-parse --show-toplevel));$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -ne 0){throw 'Missing fixed source checkout'}
if(@(& git -C $root status --porcelain).Count -ne 0){throw 'Final link requires a clean fixed source checkout'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-wddm-linked'}
$out=[IO.Path]::GetFullPath($OutputDirectory);$modules=[IO.Path]::GetFullPath($ModuleDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $out -eq $modules){throw 'Use a distinct output directory inside checkout'}
$moduleManifest=Get-Content -Raw (Join-Path $modules 'build-manifest.json')|ConvertFrom-Json
if($moduleManifest.source_commit -ne $commit -or $moduleManifest.source_dirty){throw 'Module archive must match this clean fixed source; use incremental build for changed TUs'}
foreach($source in $moduleManifest.sources){
 $file=Join-Path $root $source.path
 if(!(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -ne $source.sha256){throw 'Module source hash differs from fixed checkout'}
}
$inputs=@(@{path=$NativeOwnerLibrary;hash=$NativeOwnerSha256;role='real native owner factory and core'},@{path=$FrontendLibrary;hash=$FrontendSha256;role='real shader/caps UMD frontend'})
foreach($name in @('ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj')){
 $entry=@($moduleManifest.outputs|Where-Object {$_.name -eq $name})
 if($entry.Count -ne 1){throw "Missing module provenance: $name"}
 $inputs+=@{path=(Join-Path $modules $name);hash=$entry[0].sha256;role='verified sidecar module'}
}
$compilerManifestPath=[IO.Path]::GetFullPath($CompilerLinkManifest)
if($CompilerLinkManifestSha256 -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash $compilerManifestPath -Algorithm SHA256).Hash -ne $CompilerLinkManifestSha256){throw 'Compiler closure manifest hash mismatch'}
$compilerManifest=Get-Content -Raw $compilerManifestPath|ConvertFrom-Json
if($compilerManifest.exit_code -ne 0 -or !$compilerManifest.compiler_component_only -or $compilerManifest.crt_entry -ne '_DllMainCRTStartup'){throw 'Require actual ARM compiler DLL dependency closure with real CRT startup'}
$compilerLibraries=@()
$expectedCompilerNames=@('ge8300-pco-arm32.lib','ge8300-nir-arm32.lib','ge8300-util-arm32.lib','ge8300-support-arm32.lib','libcmt.lib','libvcruntime.lib','libcpmt.lib','libucrt.lib','kernel32.lib','synchronization.lib')
foreach($inputLibrary in $compilerManifest.inputs){
 $leaf=[IO.Path]::GetFileName($inputLibrary.path)
 if($leaf -notin $expectedCompilerNames -or $leaf -in @($compilerLibraries|ForEach-Object {[IO.Path]::GetFileName($_)})){throw 'Unexpected or duplicate compiler closure input'}
 $libraryPath=$inputLibrary.path
 if($CompilerLibraryDirectory){$libraryPath=Join-Path $CompilerLibraryDirectory $leaf}
 $compilerLibraries+=[IO.Path]::GetFullPath($libraryPath)
 $inputs+=@{path=$libraryPath;hash=$inputLibrary.sha256;role='verified user-mode compiler / Microsoft ARM CRT'}
}
if($compilerLibraries.Count -ne $expectedCompilerNames.Count){throw 'Incomplete compiler closure'}
foreach($item in $inputs){
 $item.path=[IO.Path]::GetFullPath($item.path)
 if($item.hash -notmatch '^[0-9a-fA-F]{64}$' -or !(Test-Path $item.path) -or (Get-FileHash $item.path -Algorithm SHA256).Hash -ne $item.hash){throw 'Fixed link input missing or hash mismatch'}
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$km=Join-Path $kit 'Lib\winv6.3\km\arm';$um=Join-Path $kit 'Lib\winv6.3\um\arm'
$nativeLibrary=$inputs[0].path;$frontendLibrary=$inputs[1].path
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Start-Transcript -Path (Join-Path $out 'linked-build.log') -Append|Out-Null;$transcript=$true
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$modules\ge8300-kmd-clear.lib" $nativeLibrary "$km\displib.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\libcntpr.lib" "$km\bufferoverflowfastfailk.lib" "/out:$out\ge8300-render.sys" "/map:$out\ge8300-render.map"
 if($LASTEXITCODE -ne 0){throw 'Render SYS link failed: actual native factory/control is required'}
 & lld-link /dll /machine:arm /subsystem:windows,6.03 /entry:_DllMainCRTStartup /nodefaultlib "$modules\umd_entry.obj" "$modules\ge8300-umd-clear.lib" $frontendLibrary @compilerLibraries "/out:$out\ge8300-umd.dll" "/map:$out\ge8300-umd.map"
 if($LASTEXITCODE -ne 0){throw 'UMD link failed: real geo_ge8300_frontend must implement shader/caps contract'}
 foreach($file in @('ge8300-render.sys','ge8300-umd.dll')) {
  $pe=@(& llvm-readobj --file-headers --coff-imports --coff-exports (Join-Path $out $file))
  if($LASTEXITCODE -ne 0 -or !($pe -match 'IMAGE_FILE_MACHINE_ARMNT')){throw 'Linked artifact is not verified ARMNT'}
  if($file -eq 'ge8300-render.sys' -and $pe -match '(?i)Name: Wdf'){throw 'Render SYS must not import KMDF'}
  $pe|Set-Content (Join-Path $out "$file.pe.txt")
 }
 foreach($item in $inputs){if((Get-FileHash $item.path -Algorithm SHA256).Hash -ne $item.hash){throw 'Link input changed during link'}}
 @{source_commit=$commit;module_manifest_sha256=(Get-FileHash (Join-Path $modules 'build-manifest.json') -Algorithm SHA256).Hash;inputs=$inputs;
  reused_module_origins=$moduleManifest.reuse_records;compilation_performed=$false;
  compiler_manifest_sha256=$CompilerLinkManifestSha256;user_crt_entry='_DllMainCRTStartup';
  hardware='PowerVR GE8300 / ACPI MOU0006, exclusive KMDF replacement';
  display_policy='separate MOU0007 CPU-present DOD profile; see DOD-COEXISTENCE.md';
  default_off=$true;runtime_verified=$false;signed=$false;installed=$false;
  outputs=@('ge8300-render.sys','ge8300-umd.dll')|ForEach-Object {@{name=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}}
 }|ConvertTo-Json -Depth 7|Set-Content "$out\linked-manifest.json"
} finally {if($transcript){Stop-Transcript|Out-Null};$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
