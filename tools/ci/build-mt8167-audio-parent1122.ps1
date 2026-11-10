param(
 [Parameter(Mandatory=$true)][string]$ReuseDirectory,
 [Parameter(Mandatory=$true)][string]$Parent1121Objects,
 [Parameter(Mandatory=$true)][string]$KitInclude,
 [Parameter(Mandatory=$true)][string]$UmInclude,
 [Parameter(Mandatory=$true)][string]$WdfInclude,
 [Parameter(Mandatory=$true)][string]$KernelLibraryDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$Clang='clang',[string]$Linker='lld-link',[string]$ReadObj='llvm-readobj'
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh checkout-local output required'}
if(@(& git -C $root status --porcelain).Count -or $LASTEXITCODE){throw 'Clean fixed source required'}
$commit=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'Missing source identity'}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function RequireHash($p,$h){if(!(Test-Path -LiteralPath $p -PathType Leaf) -or (Sha $p) -cne $h){throw "Pinned input changed: $p"}}
$lockPath=Join-Path $PSScriptRoot 'baselines\audio1122-parent-cache.json'
$lock=Get-Content -LiteralPath $lockPath -Raw|ConvertFrom-Json
if($lock.schema -cne 'MT8167_AUDIO1122_PARENT_CACHE_V1' -or @($lock.reused_parent_helpers).Count -ne 13){throw 'Unexpected parent cache closure'}
$compiler=& $Clang --version
if($LASTEXITCODE -or $compiler[0] -cne $lock.compiler_first_line){throw 'Use existing pinned clang20.1.8; do not setup a new SDK'}
$objects=@();$reused=@()
foreach($row in $lock.reused_parent_helpers){
 $obj="$ReuseDirectory\parent\$($row.name).obj";RequireHash $obj $row.sha256
 foreach($dep in $row.dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}
 $objects+=$obj;$reused+=@{name=$row.name;sha256=$row.sha256;dependencies=$row.dependencies}
}
$clock="$Parent1121Objects\parent-clock_snapshot.obj";RequireHash $clock $lock.clock_snapshot.object_sha256
foreach($dep in $lock.clock_snapshot.project_dependencies.PSObject.Properties){RequireHash (Join-Path $root $dep.Name) $dep.Value}
$objects+=$clock
foreach($entry in $lock.sdk_headers.PSObject.Properties){
 $parts=$entry.Name -split '/',2
 $base=switch($parts[0]){'km'{Join-Path $KitInclude 'km'} 'shared'{Join-Path $KitInclude 'shared'} 'um'{$UmInclude} 'wdf'{$WdfInclude} default{throw 'Unknown SDK scope'}}
 RequireHash (Join-Path $base $parts[1]) $entry.Value
}
$libs=@();foreach($entry in $lock.kernel_libraries.PSObject.Properties){$p=Join-Path $KernelLibraryDirectory $entry.Name;RequireHash $p $entry.Value;$libs+=$p}
foreach($entry in $lock.wdf_libraries.PSObject.Properties){$p="$ReuseDirectory\lib\wdf\$($entry.Name)";RequireHash $p $entry.Value;$libs+=$p}
$compileArgs=@();foreach($a in $lock.parent_args){
 $compileArgs+=$a.Replace('@KM_INCLUDE@',"$KitInclude\km").Replace('@SHARED_INCLUDE@',"$KitInclude\shared").Replace('@UM_INCLUDE@',$UmInclude).Replace('@WDF_INCLUDE@',$WdfInclude)
}
$compileArgs+='-DA04_SHARED_AUDIO_LIFECYCLE=1'
New-Item -ItemType Directory "$out\parent","$out\obj","$out\logs","$out\tmp"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$env:TEMP="$out\tmp";$env:TMP=$env:TEMP
try {
 $obj="$out\obj\parent-parent.obj"
 $compileArgs+=@('-MD','-MF',"$out\obj\parent-parent.d",'-c',"$root\drivers\bus\mt8167-shared\wdf_parent_probe.c",'-o',$obj)
 & $Clang @compileArgs > "$out\logs\compile-parent.log" 2>&1
 if($LASTEXITCODE){throw 'Parent TU compile failed'}
 $objects=@($obj)+$objects;$sys="$out\parent\mt8167-shared.sys"
 $linkArgs=@('/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib')+@($objects)+@($libs)+@("/out:$sys","/map:$out\logs\parent.map")
 & $Linker @linkArgs > "$out\logs\link-parent.log" 2>&1
 if($LASTEXITCODE){throw 'Parent link failed'}
 $pe=& $ReadObj --file-headers --coff-imports $sys
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT parent required'}
 $pe|Set-Content -Encoding UTF8 "$out\logs\parent-pe.txt"
 Copy-Item "$root\drivers\bus\mt8167-shared\prototype\shared-parent-audio1122.inf" "$out\parent"
 $sources=@{};foreach($p in @(& git -C $root ls-files 'drivers/bus/mt8167-shared' 'drivers/input/a04br3-buttons/pmic_read.*')){$sources[$p]=Sha (Join-Path $root $p)}
 $tools=@{};foreach($name in @($Clang,$Linker,$ReadObj)){$p=(Get-Command $name -CommandType Application).Source;$tools[$p]=Sha $p}
 @{schema='MT8167_AUDIO_LIFECYCLE_PARENT1122_UNSIGNED_V1';source_commit=$commit;lifecycle_version=1;parent_version='2026.11.10.1122';source_files=$sources;baseline_lock_sha256=(Sha $lockPath);compiler=$compiler;tools=$tools;compile_arguments=$compileArgs;link_arguments=$linkArgs;compiled_parent_TUs=1;reused_helpers=$reused;reused_clock_snapshot_sha256=(Sha $clock);USB_HCD_GPU_rebuilt=$false;USB_owner_ABI=3;SDK_rebuilt=$false;hardware_verified=$false;signed=$false;files=@(@{path='parent/mt8167-shared.sys';sha256=(Sha $sys)},@{path='parent/shared-parent-audio1122.inf';sha256=(Sha "$out\parent\shared-parent-audio1122.inf")})}|ConvertTo-Json -Depth 20|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT8167_AUDIO_PARENT1122_UNSIGNED_1TU_NOT_DEVICE_VERIFIED'
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
