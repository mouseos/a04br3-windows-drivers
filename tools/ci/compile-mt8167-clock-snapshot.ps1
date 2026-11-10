# Compile-only incremental recipe. Never installs kits, uses SSH, links,
# signs, starts a device, edits an existing package or runs an ARM image.
param([Parameter(Mandatory=$true)][string]$ParentManifest,
 [Parameter(Mandatory=$true)][string]$ThermalManifest,
 [Parameter(Mandatory=$true)][string]$KitInclude,
 [Parameter(Mandatory=$true)][string]$UmInclude,
 [Parameter(Mandatory=$true)][string]$WdfInclude,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$Compile,[string[]]$Only)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New checkout-local output required'}
$pins=Get-Content "$PSScriptRoot\clock-snapshot-baseline-inputs.json" -Raw|ConvertFrom-Json
function Sha($p){(Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Sha $ParentManifest) -cne $pins.inputs.parent.sha256 -or
 (Sha $ThermalManifest) -cne $pins.inputs.thermal.sha256){throw 'Signed baseline manifest mismatch'}
$parent=Get-Content $ParentManifest -Raw|ConvertFrom-Json
$leaf=Get-Content $ThermalManifest -Raw|ConvertFrom-Json
if($parent.source_commit -cne $pins.parent_source -or $leaf.source_commit -cne $pins.thermal_source){throw 'Baseline source mismatch'}
function MapInclude($p){
 $p=$p.Replace('\\','\')
 if($p -match '\\Include\\wdf\\kmdf\\1.13$'){return $WdfInclude}
 if($p -match '\\Include\\um$'){return $UmInclude}
 if($p -match '\\Include\\(km|shared)$'){return (Join-Path $KitInclude $Matches[1])}
 throw 'Unexpected baseline include directory'
}
function RemapFlags($flags){
 $result=@()
 for($i=0;$i -lt $flags.Count;$i++){
  if($flags[$i] -eq '-isystem'){
   if(++$i -ge $flags.Count){throw 'Truncated include flag'}
   $result+=@('-isystem',(MapInclude $flags[$i]))
  }else{$result+=$flags[$i]}
 }
 return $result
}
$common=@('--target=thumbv7-pc-windows-msvc','-ffreestanding','-fno-stack-protector','-fms-extensions')
$parentCommon=@($pins.parent_compile_common)
$parentFlags=(RemapFlags -flags @($parent.kernel_flags))+@($parent.parent_flags)
$leafFlags=RemapFlags -flags @($leaf.flags)
$opt=@('-DMT8167_CLOCK_SNAPSHOT_QUERY=1')
$rows=@(
 @{name='parent';source='drivers/bus/mt8167-shared/wdf_parent_probe.c';flags=$parentCommon+$parentFlags+$opt+@('-w')},
 @{name='clock_snapshot';source='drivers/bus/mt8167-shared/clock_snapshot.c';flags=$parentCommon+@('-std=c11','-O2','-fno-vectorize','-fno-slp-vectorize','-Wall','-Wextra','-Werror')},
 @{name='parent_snapshot_client';source='drivers/system/mt8167-clock/parent_snapshot_client.c';flags=$leafFlags+$opt},
 @{name='wdf_native_thermal';source='drivers/power/mt8167-thermal/wdf_native_thermal.c';flags=$leafFlags+$opt},
 @{name='mt8167-clock-snapshot';source='tools/clock/mt8167-clock-snapshot.c';flags=$common+@('-O2','-fno-builtin','-w','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000',"-I$UmInclude","-I$KitInclude\shared")},
 @{name='parent-flag-off';source='drivers/bus/mt8167-shared/wdf_parent_probe.c';flags=$parentCommon+$parentFlags+@('-DMT8167_CLOCK_SNAPSHOT_QUERY=0','-w')},
 @{name='thermal-flag-off';source='drivers/power/mt8167-thermal/wdf_native_thermal.c';flags=$leafFlags+@('-DMT8167_CLOCK_SNAPSHOT_QUERY=0')}
)
if($Only){
 if(@($Only|Where-Object{$_ -notin @($rows.name)}).Count){throw 'Unknown compile target'}
 $rows=@($rows|Where-Object{$_.name -in $Only})
}
if($Compile){
 if(@(& git -C $root status --porcelain).Count){throw 'Clean committed source required'}
 if(((& clang --version)[0]) -cne ($parent.compiler -split "`n")[0]){throw 'Reuse the baseline compiler; do not setup a new one'}
 # Only headers are required: no SDK installation or library restoration.
 foreach($x in $parent.sdk){
  $path=$x.path.Replace('\\','\');$local=$null
  if($path -match '\\Include\\wdf\\kmdf\\1.13\\(.+)$'){$local=Join-Path $WdfInclude $Matches[1]}
  elseif($path -match '\\Include\\um\\(.+)$'){$local=Join-Path $UmInclude $Matches[1]}
  elseif($path -match '\\Include\\(km|shared)\\(.+)$'){$local=Join-Path (Join-Path $KitInclude $Matches[1]) $Matches[2]}
  if($local -and (!(Test-Path $local) -or (Sha $local) -cne $x.sha256)){throw 'Existing WDK header closure mismatch'}
 }
}
New-Item -ItemType Directory $out,"$out\tmp"|Out-Null
$env:TMP="$out\tmp";$env:TEMP=$env:TMP
$commands=@()
foreach($row in $rows){
 $compileArguments=@($row.flags)+@('-c',(Join-Path $root $row.source),'-o',"$out\$($row.name).obj")
 $commands+=@{tool='clang';source=$row.source;source_sha256=(Sha (Join-Path $root $row.source));args=$compileArguments}
 if($Compile){& clang @compileArguments;if($LASTEXITCODE){throw "Compile failed: $($row.name)"}}
}
@{schema='MT8167_CLOCK_SNAPSHOT_COMPILE_ONLY_V1';source_commit=(& git -C $root rev-parse HEAD).Trim();
 compiled=[bool]$Compile;commands=$commands;parent_helpers_rebuild=0;thermal_helpers_rebuild=0;
 SYS_CAT_link_sign_performed=$false;deployment=$false;hardware_test=$false;runner_access=$false;
 parent_resource_contract=1117;thermal_v3_build=1104;headphone_v2_build=1107;
 integration='Parent: new parent.obj + clock_snapshot.obj + original13 helpers. Leaf: new wdf_native_thermal.obj + parent_snapshot_client.obj + unchanged actual1.10 helpers. Do not call the old full build helper or reuse old INF versions as a release.'
}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\compile-recipe.json"
if($Compile){'COMPILE_ONLY_NOT_LINKED_SIGNED_OR_DEVICE_TESTED'}else{'RECIPE_ONLY_NO_COMPILER_OR_RUNNER_USED'}
