param(
 [Parameter(Mandatory=$true)][string]$WdkInclude,
 [Parameter(Mandatory=$true)][string]$WdkArmLib,
 [Parameter(Mandatory=$true)][string]$PortClsImportLibrary,
 [Parameter(Mandatory=$true)][string]$LifecycleParent,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$UmInclude,[string]$Clang='clang',[string]$Linker='lld-link',[string]$ReadObj='llvm-readobj',[string]$DllTool='llvm-dlltool'
)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Fresh source-local output required'}
$commit=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -or @(& git -C $root status --porcelain).Count){throw 'Clean fixed source required'}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$parentManifest=Join-Path $LifecycleParent 'build-manifest.json'
$parentBuild=Get-Content -LiteralPath $parentManifest -Raw|ConvertFrom-Json
if($parentBuild.schema -cne 'MT8167_AUDIO_LIFECYCLE_PARENT1122_UNSIGNED_V1' -or $parentBuild.source_commit -cne $commit -or $parentBuild.lifecycle_version -ne 1){throw 'Matched fixed-source parent1122 build required'}
foreach($f in $parentBuild.files){if((Sha (Join-Path $LifecycleParent $f.path)) -cne $f.sha256){throw 'Parent output changed since build; sign/package after both builds'}}
$compiler=& $Clang --version
if($LASTEXITCODE -or $compiler[0] -cne 'clang version 20.1.8'){throw 'Existing pinned clang20.1.8 required; do not rebuild setup'}
$parent=Join-Path $LifecycleParent 'parent\mt8167-shared.sys'
if(!$UmInclude){$UmInclude=Join-Path $WdkInclude 'um'}
$tools=@{}
foreach($pair in @(@('clang',$Clang),@('linker',$Linker),@('readobj',$ReadObj))){
 $p=(Get-Command $pair[1] -CommandType Application -ErrorAction Stop).Source
 $tools[$pair[0]]=@{path=$p;sha256=(Sha $p)}
}
$inputs=@{}
foreach($p in @($PortClsImportLibrary,(Join-Path $WdkInclude 'km\portcls.h'),(Join-Path $WdkInclude 'km\ntddk.h'),(Join-Path $WdkInclude 'shared\ksmedia.h'))){$inputs[$p]=Sha $p}
foreach($name in @('ntoskrnl','hal','bufferoverflowfastfailk','libcntpr')){$p=Join-Path $WdkArmLib ($name+'.lib');$inputs[$p]=Sha $p}
# Native speaker lifecycle profile; optional HP/capture profiles keep their own contracts.
# No shared-parent/peripheral/EDK2 compiler is invoked and no cached OBJ is used.
$flags=@('A04_MT8167_WAVERT_EXPERIMENTAL','A04_MT8167_WAVERT_DIAGNOSTIC','A04_MT8167_OWNER_DIAGNOSTIC','A04_AUDIO_QUERY_SHARED','A04_AUDIO_MAP_AFE','A04_AUDIO_IRQ_DIAGNOSTIC','A04_AUDIO_RUN_DIAGNOSTIC','A04_AUDIO_DIGITAL_PLAYBACK','A04_AUDIO_ENGINE_PCM','A04_AUDIO_VOICE_PCM_DIAGNOSTIC','A04_AUDIO_WAVE_DMA_RETAINED','A04_AUDIO_START_PERSIST','A04_AUDIO_DIAGNOSTIC_TRACE','A04_AUDIO_DMA_NOTIFICATION','A04_AUDIO_LIFECYCLE')|ForEach-Object{'-D'+$_+'=1'}
$common=@('--target=thumbv7-pc-windows-msvc','-w','-O2','-fms-extensions','-ffreestanding','-fno-stack-protector','-fno-builtin','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D__ldrexd=__a04_thermal1101_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-isystem',"$WdkInclude\km",'-isystem',"$WdkInclude\shared",'-isystem',$UmInclude)
$c=@('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query','capture_parent_query','ul_engine')
$cpp=@('adapter','wavert_miniport','wavert_registration','pnp_backend','topology_miniport','headphone_endpoint','wavert_dma','common_dma','dma_adapter','stream_transport','afe_mapping','irq_windows','wave_dma_live','start_record','capture_owner')
New-Item -ItemType Directory "$out\obj","$out\logs","$out\tmp","$out\audio"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$env:TEMP="$out\tmp";$env:TMP="$out\tmp"
try {
 $source=@{}
 foreach($p in @(& git -C $root ls-files 'drivers/audio/mt8167-afe' 'drivers/bus/mt8167-shared/*.h' 'drivers/bus/mt8167-shared/clock_owner.c')){$source[$p]=Sha (Join-Path $root $p)}
 $units=@();foreach($name in $c){$units+=@{name=$name;path="drivers/audio/mt8167-afe/$name.c";cpp=$false}}
 $units+=@{name='clock_owner';path='drivers/bus/mt8167-shared/clock_owner.c';cpp=$false}
 foreach($name in $cpp){$units+=@{name=$name;path="drivers/audio/mt8167-afe/$name.cpp";cpp=$true}}
 if($units.Count -ne 23){throw 'Expected23 source units'}
 $objects=@();$records=@()
 foreach($u in $units){
  $obj="$out\obj\$($u.name).obj";$compileArgs=@($common)+@($flags)
  if($u.cpp){$compileArgs+=@('-std=c++14','-fms-compatibility','-fno-exceptions','-fno-rtti')}else{$compileArgs+='-std=c11'}
  $compileArgs+=@('-MMD','-MF',"$out\obj\$($u.name).d",'-c',(Join-Path $root $u.path),'-o',$obj)
  & $Clang @compileArgs > "$out\logs\compile-$($u.name).log" 2>&1
  if($LASTEXITCODE){throw "Compile failed: $($u.name)"}
  $objects+=$obj;$records+=@{source=$u.path;object_sha256=(Sha $obj);arguments=$compileArgs}
 }
 $sys="$out\audio\mt8167-audio.sys"
 $extraImports=@()
 $archiveText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($PortClsImportLibrary))
 if(!$archiveText.Contains('PcRegisterAdapterPowerManagement') -or !$archiveText.Contains('PcUnregisterAdapterPowerManagement')) {
  # Both names are exports of saved installed RT8.1 ARM portcls.sys SHA
  # f9d0f5ce9d6072785a9cf8d242c3f490e4b2af6455ff05fec6a21815bc3e9db6.
  # Import thunks only, never a replacement PortCls binary or SDK rebuild.
  $def="$out\obj\portcls-power.def";$imp="$out\obj\portcls-power.lib"
  "LIBRARY portcls.sys`nEXPORTS`nPcRegisterAdapterPowerManagement`nPcUnregisterAdapterPowerManagement`n"|Set-Content -Encoding ASCII $def
  & $DllTool -m arm -d $def -l $imp > "$out\logs\power-import-lib.log" 2>&1
  if($LASTEXITCODE){throw 'Power import library generation failed'}
  $extraImports+=$imp;$inputs[$imp]=Sha $imp
  $dllToolPath=(Get-Command $DllTool -CommandType Application).Source;$tools['dlltool']=@{path=$dllToolPath;sha256=(Sha $dllToolPath)}
 }
 $linkArgs=@('/driver','/subsystem:native','/machine:arm','/entry:DriverEntry','/nodefaultlib')+@($objects)+@($PortClsImportLibrary)+$extraImports
 foreach($name in @('ntoskrnl','hal','bufferoverflowfastfailk','libcntpr')){$linkArgs+=Join-Path $WdkArmLib ($name+'.lib')}
 $linkArgs+="/out:$sys";& $Linker @linkArgs > "$out\logs\link.log" 2>&1
 if($LASTEXITCODE){throw 'Audio link failed'}
 Copy-Item "$root\drivers\audio\mt8167-afe\mt8167-afe-speaker1036.inf" "$out\audio"
 $pe=& $ReadObj --file-headers --coff-imports $sys
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'ARMNT required'}
 $pe|Set-Content -Encoding UTF8 "$out\logs\audio-pe.txt"
 # The consumer may still use polling. No new client EXE or synthetic tone.
 @{schema='mt8167-mt6392-wavert1036-unsigned';source_commit=$commit;source_files=$source;tools=$tools;inputs=$inputs;audio_flags=$flags;objects=$records;link_arguments=$linkArgs;parent_lifecycle_manifest_sha256=(Sha $parentManifest);parent_SYS_unsigned_sha256=(Sha $parent);parent_rebuilt_separately_1TU=$true;SDK_rebuilt=$false;hardware_verified=$false;audible_verified=$false;source_stop_release_protocol_complete=$true;physical_DMA_bus_idle_measured=$false;production_hardware_verified=$false;files=@(@{path='audio/mt8167-audio.sys';sha256=(Sha $sys)},@{path='audio/mt8167-afe-speaker1036.inf';sha256=(Sha "$out\audio\mt8167-afe-speaker1036.inf")})}|ConvertTo-Json -Depth 16|Set-Content -Encoding UTF8 "$out\build-manifest.json"
 'MT8167_MT6392_WAVERT1036_UNSIGNED_NOT_DEVICE_VERIFIED'
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
