# KMDF-free native owner module build only. No installed GPU package is made.
param(
 [switch]$EnableCandidate,
 [string]$ExpectedSourceCommit,
 [string]$OutputDirectory,
 [string]$WdkRoot="${env:ProgramFiles(x86)}\Windows Kits\8.1",
 [switch]$LinkCheck,
 [switch]$WithMdlTarget
)
$ErrorActionPreference='Stop'
if(!$EnableCandidate){throw 'Native owner module build is default off; pass -EnableCandidate'}
if([string]::IsNullOrWhiteSpace($ExpectedSourceCommit)){throw 'ExpectedSourceCommit is required; no implicit HEAD build'}
$root=(& git -C $PSScriptRoot rev-parse --show-toplevel)
if($LASTEXITCODE -ne 0){throw 'Cannot resolve source checkout'}
$root=[IO.Path]::GetFullPath($root)
$commit=(& git -C $root rev-parse HEAD)
$expected=(& git -C $root rev-parse --verify "$($ExpectedSourceCommit)^{commit}")
if($LASTEXITCODE -ne 0 -or $commit -ne $expected){throw 'ExpectedSourceCommit must resolve to the current HEAD'}
$dirty=@(& git -C $root status --porcelain)
if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Require a clean fixed source checkout'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\ge8300-native-owner-core'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must stay inside checkout'}
if(Test-Path $out){throw 'Choose a new output directory; existing artifacts are never reused'}
$inc=Join-Path $WdkRoot 'Include';$kmLib=Join-Path $WdkRoot 'Lib\winv6.3\km\arm'
foreach($file in @((Join-Path $inc 'km\ntddk.h'),(Join-Path $kmLib 'ntoskrnl.lib'),(Join-Path $kmLib 'hal.lib'),(Join-Path $kmLib 'libcntpr.lib'),(Join-Path $kmLib 'bufferoverflowfastfailk.lib'))){
 if(!(Test-Path $file)){throw "Missing real WDK8.1 ARM input: $file"}
}
foreach($tool in @('clang','llvm-lib','llvm-readobj')){if(!(Get-Command $tool -ErrorAction SilentlyContinue)){throw "Missing LLVM tool: $tool"}}
if($LinkCheck -and !(Get-Command lld-link -ErrorAction SilentlyContinue)){throw 'Missing lld-link'}
$resource=(& clang -print-resource-dir)
if($LASTEXITCODE -ne 0 -or !(Test-Path (Join-Path $resource 'include\intrin.h'))){throw 'Missing matching Clang intrinsic headers'}
$gpu=Join-Path $root 'drivers\gpu\mt8167'
$units=@('dma_pages','windows_hw','vendor_m1_11\native_owner_mdl','vendor_m1_11\native_owner_irq','vendor_m1_11\windows_native_submit')
$common=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-fms-extensions','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none',
 '-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=return-type',
 '-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1',
 '-D__ldrexd=__geo_wdk81_ldrexd_declaration',"-I$resource\include","-I$inc\km","-I$inc\km\crt","-I$inc\shared","-I$gpu","-I$gpu\fwif","-I$gpu\compat","-I$gpu\vendor_m1_11")
if($WithMdlTarget){
 $units+=@('vendor_m1_11\native_owner_dispatch','vendor_m1_11\windows_native_target','vendor_m1_11\windows_native_storage','vendor_m1_11\windows_native_cold','vendor_m1_11\windows_native_code','vendor_m1_11\windows_native_backing','vendor_m1_11\windows_native_job','vendor_m1_11\windows_native_execute','vendor_m1_11\windows_uncached','vendor_m1_11\native_resources','vendor_m1_11\native_hwrt_profile','vendor_m1_11\render_resources','vendor_m1_11\native_clear_job','vendor_m1_11\native_clear_data','vendor_m1_11\native_terminate_stream','vendor_m1_11\native_clear_surface','vendor_m1_11\native_clear_codespace','vendor_m1_11\native_clear_pds','vendor_m1_11\native_eot_program','vendor_m1_11\native_record_fields','vendor_m1_11\native_pair','vendor_m1_11\native_ccb')
 $common+='-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1'
}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$transcript=$false
New-Item -ItemType Directory $out,(Join-Path $out 'tmp')|Out-Null
try {
 $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
 Start-Transcript -Path (Join-Path $out 'build.log')|Out-Null;$transcript=$true
 $commands=@();$objects=@()
 foreach($unit in $units){
  $source=Join-Path $gpu "$unit.c";$obj=Join-Path $out ($unit.Replace('\','-')+'.obj')
  $compileArgs=@($common)+@('-c',$source,'-o',$obj)
  & clang @compileArgs
  if($LASTEXITCODE -ne 0){throw "ARM32 native core compile failed: $unit"}
  $commands+=,@{tool='clang';arguments=$compileArgs};$objects+=$obj
 }
 & llvm-lib /machine:arm "/out:$out\ge8300-native-owner-core.lib" @objects
 if($LASTEXITCODE -ne 0){throw 'Native owner archive failed'}
 $objectInfo=(& llvm-readobj --file-headers --symbols @objects)
 if($LASTEXITCODE -ne 0){throw 'COFF inspection failed'}
 $objectInfo|Set-Content "$out\objects.txt"
 if(@($objectInfo|Where-Object{$_ -match 'Machine:.*(ARMNT|0x1C4)'}).Count -ne $objects.Count){throw 'Every object must be real ARMNT COFF'}
 $outputs=@($objects)+@((Join-Path $out 'ge8300-native-owner-core.lib'))
 if($LinkCheck){
  # Export real functions from all objects so /opt:ref cannot hide unresolved
  # code. No DriverEntry, dummy callbacks or unresolved-symbol override.
  $exports=@('a04_native_mdl_bind','a04_native_mdl_pages','a04_native_mdl_cpu','a04_native_mdl_original','a04_native_mdl_release_cold','a04_native_mdl_sync_for_gpu','a04_native_mdl_sync_for_cpu','a04_native_mdl_mark_exposed',
   'a04_native_irq_bind','a04_native_irq_claim','a04_native_irq_rearm','a04_native_irq_mask_close','a04_native_irq_unbind_disconnected','a04_vendor_native_submit_prepared_pair','a04_gpu_dma_barrier')
  if($WithMdlTarget){$exports+=@('a04_native_dispatch_initialize','a04_native_dispatch_submit','a04_native_dispatch_interrupt','a04_native_dispatch_service','a04_vendor_native_attach_mdl_target_cold','a04_vendor_native_attach_mdl_target_at_cold','a04_vendor_native_target_aperture','a04_vendor_native_target_before_fw_start','a04_vendor_native_allocate_cold_ex','a04_vendor_native_run_clear_frames','a04_vendor_native_owner_submit','a04_vendor_native_owner_finish_passive','a04_vendor_native_owner_arm_visibility','a04_vendor_native_owner_interrupt_event')}
  $link=@('/dll','/noentry','/subsystem:native','/machine:arm','/nodefaultlib')+$objects
  $link+=@((Join-Path $kmLib 'ntoskrnl.lib'),(Join-Path $kmLib 'hal.lib'),(Join-Path $kmLib 'libcntpr.lib'),(Join-Path $kmLib 'bufferoverflowfastfailk.lib'))
  $link+=@($exports|ForEach-Object{"/export:$_"});$link+="/out:$out\owner-core-linkcheck.dll"
  & lld-link @link
  if($LASTEXITCODE -ne 0){throw 'Native owner real WDK API link check failed'}
  $commands+=,@{tool='lld-link';arguments=$link}
  $imports=(& llvm-readobj --file-headers --coff-imports "$out\owner-core-linkcheck.dll")
  if($LASTEXITCODE -ne 0){throw 'Link check PE inspection failed'}
  $imports|Set-Content "$out\linkcheck-imports.txt"
  if(!@($imports|Where-Object{$_ -match 'Machine:.*(ARMNT|0x1C4)'}).Count){throw 'Link check must be real ARMNT PE'}
  foreach($line in $imports){if($line -match '^\s*Name:\s*(\S+)\s*$' -and $Matches[1] -notin @('ntoskrnl.exe','hal.dll')){throw "Unexpected non-kernel import: $($Matches[1])"}}
  $outputs+=Join-Path $out 'owner-core-linkcheck.dll'
 }
 $toolHashes=@('clang','llvm-lib','llvm-readobj')
 if($LinkCheck){$toolHashes+='lld-link'}
 $toolHashes=@($toolHashes|ForEach-Object{$path=(Get-Command $_).Source;@{name=$_;path=$path;sha256=(Get-FileHash $path -Algorithm SHA256).Hash}})
 $wdkHashes=@('ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')|ForEach-Object{$path=Join-Path $kmLib $_;@{path=$path;sha256=(Get-FileHash $path -Algorithm SHA256).Hash}}
 $wdkHashes+=@{path=(Join-Path $inc 'km\ntddk.h');sha256=(Get-FileHash (Join-Path $inc 'km\ntddk.h') -Algorithm SHA256).Hash}
 $sources=@(Get-ChildItem $gpu -File -Recurse|Where-Object{$_.Extension -in '.c','.h'}|ForEach-Object{@{path=$_.FullName.Substring($root.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 $sources+=@{path=$MyInvocation.MyCommand.Path.Substring($root.Length+1);sha256=(Get-FileHash $MyInvocation.MyCommand.Path -Algorithm SHA256).Hash}
 $productHashes=@($outputs|ForEach-Object{@{path=$_.Substring($out.Length+1);sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}})
 @{source_commit=$commit;source_dirty=$false;candidate_default_off=$true;wdk_root=$WdkRoot;target='thumbv7-pc-windows-msvc';kmdf_dependency=$false;units=$units;compile_flags=$common;commands=$commands;tools=$toolHashes;wdk_inputs=$wdkHashes;
  intrinsic_compat='obsolete WDK __ldrexd declaration-only alias, matching H c1a4a81; no replacement intrinsic implementation';
  linkcheck_requested=[bool]$LinkCheck;mdl_target_requested=[bool]$WithMdlTarget;kind='COFF objects/static library; optional native DLL solely for API link validation, not an installable driver';
  owner_create_delete_implemented=$false;geo_full_control_ready=$false;runtime_verified=$false;sources=$sources;outputs=$productHashes}|ConvertTo-Json -Depth 8|Set-Content "$out\build-manifest.json"
} finally {
 if($transcript){Stop-Transcript|Out-Null}
 $env:TEMP=$oldTemp;$env:TMP=$oldTmp
}
