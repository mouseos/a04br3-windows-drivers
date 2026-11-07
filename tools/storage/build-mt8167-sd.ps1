# Independent WDK8.1 ARM32 microSD profile. No eMMC/parent/GPU/audio build,
# signing, deployment, runner start or hardware action.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt8167-sd'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE){throw 'Cannot identify source commit'}
$dirty=@(& git -C $root status --porcelain);if($LASTEXITCODE -or $dirty.Count){throw 'Requires a clean fixed source commit'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$priorTmp=$env:TMP;$priorTemp=$env:TEMP
try {
 $env:TMP=Join-Path $out 'tmp';$env:TEMP=$env:TMP
 $manifest=Join-Path $out 'manifest.json'
 if(Test-Path $manifest){Move-Item $manifest (Join-Path $out ('manifest.previous.'+[Guid]::NewGuid().ToString('N')+'.json'))}
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include'
 $km=Join-Path $kit 'Lib\winv6.3\km\arm';$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
 $kernelCodegenFlags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
 # E/H WDK8.1 recipe: rename only the obsolete SDK declaration; no intrinsic stub.
 $intrinsicCompatFlags=@('-D__ldrexd=__sd_wdk81_ldrexd_declaration')
 $kernelRuntime=Join-Path $km 'libcntpr.lib'
 if(!(Test-Path -LiteralPath $kernelRuntime -PathType Leaf)){throw "Missing real WDK8.1 ARM kernel runtime: $kernelRuntime"}
 $objects=@();$sources=@()
 $paths=@('drivers/storage/mt8167-sd/consumer.c','drivers/storage/mt8167-sd/checkpoint.c','drivers/storage/mt8167-sd/sd.c','drivers/storage/mt8167-sd/windows.c','drivers/storage/mt8167-sd/scsi.c','drivers/storage/mt8167-sd/control.c','drivers/storage/mt8167-sd/power_client.c',
  'drivers/storage/mt8167-msdc/cdb.c','drivers/storage/mt8167-msdc/scsi_capacity.c')
 foreach($path in $paths){
  $obj=Join-Path $out (([IO.Path]::GetFileNameWithoutExtension($path))+'.obj');$objects+=$obj
  & clang --target=thumbv7-pc-windows-msvc -std=c11 -O2 -ffreestanding -fno-stack-protector @kernelCodegenFlags @intrinsicCompatFlags -fms-extensions -ffunction-sections -fdata-sections -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c (Join-Path $root $path) -o $obj
  if($LASTEXITCODE){throw "microSD compile failed: $path"}
  $sources+=@{path=$path;sha256=(Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib /opt:ref @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" $kernelRuntime "/out:$out\mt8167-sd.sys"
 if($LASTEXITCODE){throw 'microSD final driver link failed'}
 foreach($name in @('mt8167-sd.inf','README.md')){Copy-Item "$root\drivers\storage\mt8167-sd\$name" $out -Force}
 Copy-Item "$root\drivers\storage\mt8167-sd\acpi\MT8167-SD.asl" $out -Force
 @{source_commit=$commit;source_dirty=$false;sources=$sources;target='Windows RT8.1 ARM32';kmdf='1.13';
   kernel_codegen_flags=$kernelCodegenFlags;kernel_intrinsic_compat_flags=$intrinsicCompatFlags;
   kernel_intrinsic_compat='WDK8.1 obsolete __ldrexd declaration-only alias, matching E/H recipe; no replacement implementation';
   kernel_runtime_path='Lib/winv6.3/km/arm/libcntpr.lib';kernel_runtime_sha256=(Get-FileHash $kernelRuntime -Algorithm SHA256).Hash;
   native_checkpoint_schema=1;native_checkpoint_revision=5;dedicated_sd_consumer=$true;cancel_safe_queue=$true;queue_limit=8;shared_worker_hardware_waits=$false;add_failure_root_cause_proven=$false;sd_power_interface_version=1;power_relations_implemented=$true;
   controller='MediaTek MT8167 MSDC1 microSD';mmio='0x11130000/0x1000';gic_spi=79;gic_intid=111;cd_gpio=20;cd_present_level=1;
   profile='native SD, PIO,1-bit,host clock divisor4 (source/16),512-byte sectors';dma_enabled=$false;boot_storage=$false;
   kernel_driver_link_succeeded=$true;acpi_compiled=$false;signed=$false;windows_enumeration_verified=$false;hardware_rw_verified=$false;
   platform_dependency='Matched SDCD/SDCF and shared SD power interface v1, PowerRelations + S0 StopIdle lease';
   sha256=(Get-FileHash (Join-Path $out 'mt8167-sd.sys') -Algorithm SHA256).Hash
 }|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 $manifest
} finally {$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
