# Fixed-source WDK8.1 ARM package, sole I2C1 owner remains the MMA provider.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$SharedIdentity,[switch]$SharedCameraStream,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
if($SharedCameraStream -and !$SharedIdentity){throw 'Camera stream requires SharedIdentity'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Require a new output inside this checkout'}
$commit=(& git -C $root rev-parse HEAD);$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty -or $commit -notmatch '^[a-f0-9]{40}$'){throw 'Clean fixed source required'}
$baseline='2eea34ede8c18b4a3093de255b4f7ace9a695d0b'
foreach($name in @('mma8652.c','mma8652.h','sensor_hid.c','sensor_hid.h','mt8167_i2c.c','mt8167_i2c.h')){
 $path="drivers/sensors/mma8652/$name"
 $old=(& git -C $root rev-parse "${baseline}:$path");if($LASTEXITCODE){throw 'Baseline missing'}
 $new=(& git -C $root rev-parse "HEAD:$path");if($LASTEXITCODE -or $old -ne $new){throw "Proven MMA runtime changed: $name"}
}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$wdf="$kit\Lib\wdf\kmdf\arm\1.13"
New-Item -ItemType Directory $out,"$out\tmp","$out\package"|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$oldRunner=$env:RUNNER_TEMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$env:TEMP
 $flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-w','-fms-extensions','-ffreestanding','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13")
 $objects=@();$flag=[int][bool]$SharedIdentity;$stream=[int][bool]$SharedCameraStream
 foreach($name in @('mma8652','mt8167_i2c','sensor_hid','wdf_mma8652')){
  $obj="$out\$name.obj"
  & clang @flags "-DA04_MMA_SHARED_IDENTITY=$flag" "-DA04_MMA_SHARED_CAMERA_STREAM=$stream" -DMMA8652_UART_TRACE=0 -c "$root\drivers\sensors\mma8652\$name.c" -o $obj
  if($LASTEXITCODE){throw "Compile failed $name"};$objects+=$obj
 }
 & clang @flags -c "$root\drivers\bus\mt8167-i2c\pio.c" -o "$out\pio.obj"
 if($LASTEXITCODE){throw 'PIO compile failed'};$objects+="$out\pio.obj"
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/out:$out\package\mma8652.sys"
 if($LASTEXITCODE){throw 'MMA shared provider link failed'}
 $camera=@()
 foreach($name in @('windows_owner_client','power_sequence','hi556_identity')){
  & clang @flags -c "$root\drivers\camera\hi556\$name.c" -o "$out\$name.obj"
  if($LASTEXITCODE){throw "Camera module compile failed: $name"};$camera+="$out\$name.obj"
 }
 & lld-link /lib /machine:arm @camera "/out:$out\hi556-owner-client.lib"
 if($LASTEXITCODE){throw 'Camera modules archive failed'}
 # A library alone does not resolve imports. Link a non-installable native
 # no-entry verification image, preserving client/power exports for /opt:ref.
 & lld-link /dll /noentry /subsystem:native /machine:arm /nodefaultlib @camera "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" /export:a04_hi556_owner_open /export:a04_hi556_owner_close /export:a04_hi556_owner_identify /export:a04_hi556_power_start "/out:$out\hi556-owner-linkcheck.dll"
 if($LASTEXITCODE){throw 'Camera kernel import link check failed'}
 Copy-Item "$root\drivers\sensors\mma8652\mma8652.inf" "$out\package"
 if($SharedCameraStream){
  $inf="$out\package\mma8652.inf";[IO.File]::WriteAllText($inf,[IO.File]::ReadAllText($inf).Replace('0.0.2.0','0.0.2.1'),[Text.Encoding]::ASCII)
 }
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'Inf2Cat failed'}
 if($CertificateThumbprint){
  & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$out\package\mma8652.sys"
  if($LASTEXITCODE){throw 'SYS signing failed'}
  & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime
  if($LASTEXITCODE){throw 'Signed SYS catalog regeneration failed'}
  & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$out\package\mma8652.cat"
  if($LASTEXITCODE){throw 'CAT signing failed'}
  foreach($n in @('mma8652.sys','mma8652.inf')){
   & "$kit\bin\x86\signtool.exe" verify /pa /v /c "$out\package\mma8652.cat" "$out\package\$n"
   if($LASTEXITCODE){throw 'Membership verification failed'}
  }
 }
} finally {$env:TEMP=$oldTemp;$env:TMP=$oldTmp;$env:RUNNER_TEMP=$oldRunner}
[ordered]@{
 source_commit=$commit;source_dirty=$false;mma_runtime_baseline=$baseline
 version=$(if($SharedCameraStream){'0.0.2.1'}else{'0.0.2.0'});shared_camera_stream=[bool]$SharedCameraStream;shared_identity=[bool]$SharedIdentity;shared_identity_default=$false
 sole_mmio_owner='MMA8652 existing I2C1 FDO';transport='referenced IRP_MN_QUERY_INTERFACE'
 board_power_provider_implemented=$false;camera_bindable=$false;hardware_verified=$false
 axis_hid_identity_core_blobs_preserved=$true;critical_flags=@{A04_MMA_SHARED_IDENTITY=$flag;A04_MMA_SHARED_CAMERA_STREAM=$stream;MMA8652_UART_TRACE=0}
 kernel_codegen_flags=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none')
 signed=[bool]$CertificateThumbprint;signing_thumbprint=$CertificateThumbprint
 recipe_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash
 files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 camera_import_linkcheck_sha256=(Get-FileHash "$out\hi556-owner-linkcheck.dll" -Algorithm SHA256).Hash
 camera_library_sha256=(Get-FileHash "$out\hi556-owner-client.lib" -Algorithm SHA256).Hash
}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\manifest.json"
