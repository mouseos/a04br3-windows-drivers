# Integrated opt-in rear HI556 RAW capture candidate, not an AVStream camera.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$CaptureExperiment,[string]$SharedOwnerDirectory,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
if($SharedOwnerDirectory -and !$CaptureExperiment){throw 'Shared owner reuse requires CaptureExperiment'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'Require new output within fixed checkout'}
$commit=(& git -C $root rev-parse HEAD);$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Clean fixed source required'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm";$um="$kit\Lib\winv6.3\um\arm";$wdf="$kit\Lib\wdf\kmdf\arm\1.13"
New-Item -ItemType Directory $out,"$out\tmp","$out\package","$out\client"|Out-Null
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$env:TEMP
 $flag=[int][bool]$CaptureExperiment
 $flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-w','-ffreestanding','-fms-extensions','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_KERNEL_MODE','-D_ARM_','-DARM','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um","-I$inc\wdf\kmdf\1.13")
 $objects=@()
 foreach($name in @('wdf_capture','windows_power','windows_frame','windows_owner_client','receiver')){
  $obj="$out\$name.obj"
  & clang @flags "-DA04_HI556_CAPTURE_EXPERIMENT=$flag" -c "$root\drivers\camera\hi556\$name.c" -o $obj
  if($LASTEXITCODE){throw "HI556 compile failed: $name"};$objects+=$obj
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects "$wdf\wdfdriverentry.lib" "$wdf\wdfldr.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\hi556-capture.map" "/out:$out\package\hi556-capture.sys"
 if($LASTEXITCODE){throw 'Integrated HI556 driver link failed'}
 Copy-Item "$root\drivers\camera\hi556\hi556-capture.inf" "$out\package"
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -O2 -w -ffreestanding -fno-builtin -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$root\tools\winpe\hi556-capture.c" -o "$out\client.obj"
 if($LASTEXITCODE){throw 'HI556 explicit client compile failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\client.obj" "$um\kernel32.lib" "$um\setupapi.lib" "$um\shell32.lib" "$um\user32.lib" "/out:$out\client\hi556-capture.exe"
 if($LASTEXITCODE){throw 'HI556 explicit client link failed'}
 if($CaptureExperiment){
  if($SharedOwnerDirectory){
   $shared=[IO.Path]::GetFullPath($SharedOwnerDirectory)
   if(!$shared.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Shared owner input must be within checkout'}
   $m=Get-Content -Raw (Join-Path $shared 'manifest.json')|ConvertFrom-Json
   if($m.source_dirty -ne $false -or $m.source_commit -notmatch '^[0-9a-f]{40}$' -or $m.version -ne '0.0.2.1' -or !$m.signed -or !$m.axis_hid_identity_core_blobs_preserved){throw 'Require fixed signed shared MMA2.1'}
   if($m.critical_flags.MMA8652_UART_TRACE -ne 0){throw 'Require matching silent MMA UART policy'}
   foreach($flag in @('-mfpu=none','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access')){if($m.kernel_codegen_flags -notcontains $flag){throw 'Shared owner ARM codegen differs'}}
   foreach($flag in @('A04_MMA_SHARED_IDENTITY','A04_MMA_SHARED_CAMERA_STREAM')){if($m.critical_flags.$flag -ne 1){throw "Missing shared owner flag: $flag"}}
   foreach($path in @('drivers/bus/mt8167-i2c/shared_identity_interface.h','drivers/sensors/mma8652/shared_identity.inc','drivers/sensors/mma8652/wdf_mma8652.c','drivers/sensors/mma8652/mma8652.c','drivers/sensors/mma8652/mma8652.h','drivers/sensors/mma8652/mt8167_i2c.c','drivers/sensors/mma8652/mt8167_i2c.h','drivers/sensors/mma8652/sensor_hid.c','drivers/sensors/mma8652/sensor_hid.h','drivers/bus/mt8167-i2c/pio.c','drivers/bus/mt8167-i2c/pio.h')){
    $prior=(& git -C $root rev-parse "$($m.source_commit):$path");if($LASTEXITCODE){throw 'Shared owner source unavailable'}
    $current=(& git -C $root rev-parse "HEAD:$path");if($LASTEXITCODE -or $prior -ne $current){throw "Shared owner source differs: $path"}
   }
   New-Item -ItemType Directory "$out\mma\package"|Out-Null
   foreach($name in @('mma8652.sys','mma8652.inf','mma8652.cat')){
    $entry=@($m.files|Where-Object {$_.path -eq "package/$name"})
    if($entry.Count -ne 1 -or (Hash "$shared\package\$name") -ne $entry[0].sha256){throw "Shared owner hash mismatch: $name"}
    Copy-Item -LiteralPath "$shared\package\$name" -Destination "$out\mma\package\$name"
   }
   foreach($name in @('mma8652.sys','mma8652.inf')){
    & "$kit\bin\x86\signtool.exe" verify /pa /v /c "$out\mma\package\mma8652.cat" "$out\mma\package\$name"
    if($LASTEXITCODE){throw 'Reused MMA catalog/signature verification failed'}
   }
   $m|Add-Member -NotePropertyName reused_by_source_commit -NotePropertyValue $commit
   $m|Add-Member -NotePropertyName original_manifest_sha256 -NotePropertyValue (Hash "$shared\manifest.json")
   $m|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\mma\manifest.json"
  }else{
   & "$PSScriptRoot\build-mma8652-shared-identity.ps1" -OutputDirectory "$out\mma" -SharedIdentity -SharedCameraStream -CertificateThumbprint $CertificateThumbprint
  }
 }
 if($CertificateThumbprint){
  foreach($file in @("$out\package\hi556-capture.sys","$out\client\hi556-capture.exe")){
   & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint $file
   if($LASTEXITCODE){throw 'HI556 signing failed'}
  }
 }
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'HI556 Inf2Cat failed'}
 if($CertificateThumbprint){
  & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$out\package\hi556-capture.cat"
  if($LASTEXITCODE){throw 'HI556 CAT signing failed'}
  foreach($file in @('hi556-capture.inf','hi556-capture.sys')){
   & "$kit\bin\x86\signtool.exe" verify /pa /v /c "$out\package\hi556-capture.cat" "$out\package\$file"
   if($LASTEXITCODE){throw 'HI556 membership failed'}
  }
 }
} finally {$env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner}
[ordered]@{
 status='integrated-raw-capture-candidate-not-hardware-validated';source_commit=$commit;source_dirty=$false
 recipe_sha256=(Hash $PSCommandPath);camera_version='0.0.1.1';status_schema=2;power_stop_rundown='EvtIoStop atomic request; sole original owner completes after hardware quiesce';total_servicing_deadline_seconds=32;mma_version=$(if($CaptureExperiment){'0.0.2.1'}else{$null})
 critical_flags=@{A04_HI556_CAPTURE_EXPERIMENT=$flag;A04_MMA_SHARED_IDENTITY=$flag;A04_MMA_SHARED_CAMERA_STREAM=$flag}
 shared_owner_reused=[bool]$SharedOwnerDirectory;camera_default_enabled=$false;required_firmware='uefi-a04br3 work/windows-camera-handoff, including ISP quiesce ACK before publishing NVS'
 model='rear Hynix HI556 chip0556 I2C1 addr28';front_camera_bound=$false
 sensor_tables_manifest_sha256=(Hash "$root\drivers\camera\hi556\vendor\tables-provenance.json")
 receiver_source_sha256=(Hash "$root\drivers\camera\hi556\receiver.c")
 sole_i2c_mmio_owner='MMA8652';camera_private_mmio=@('ISP/SENINF','MIPI analog','LARB1');shared_top_gpio_pmic_windows_writes=$false
 power_mode='pinned verified boot rails/MCLK; ACPI HSTA plus referenced token; no guessed rail-off'
 dma_mode='DMA adapter v3 noncached common buffer,32bit logical; require IMGO M4U/security bypass already established'
 completion='fresh Pass1 AND IMGO done, no error, no second SOF, sensor stop + ISP reset ACK,guard intact,content changed'
 capture_on_load=$false;avstream_implemented=$false;otp_values_embedded=$false;device_calibration_embedded=$false;hardware_verified=$false
 blockers=@('No nonzero frame proven on Windows or source Linux baseline; NCSI2->MUX remains unresolved','Requires matched opt-in camera handoff firmware and MMA shared owner; not for current normal firmware','No front IC identification or AVStream/WinRT API; no fabricated OTP calibration')
 signed=[bool]$CertificateThumbprint;certificate_thumbprint=$CertificateThumbprint
 files=@(Get-ChildItem "$out\package","$out\client" -File|ForEach-Object{@{path=$_.FullName.Substring($out.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Hash $_.FullName)}})
}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\manifest.json"
