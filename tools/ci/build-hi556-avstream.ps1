# Fixed-source Windows8.1 ARM AVStream candidate; no device/runner operations.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$CaptureExperiment,[string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $out)){throw 'New output inside fixed checkout required'}
$commit=(& git -C $root rev-parse HEAD);$dirty=(& git -C $root status --porcelain --untracked-files=normal)
if($LASTEXITCODE -or $dirty -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Clean fixed source required'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc="$kit\Include";$km="$kit\Lib\winv6.3\km\arm"
New-Item -ItemType Directory $out,"$out\tmp","$out\package"|Out-Null
$priorTemp=$env:TEMP;$priorTmp=$env:TMP;$priorRunner=$env:RUNNER_TEMP
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$env:TEMP
 $flag=[int][bool]$CaptureExperiment
 $flags=@('--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-w','-ffreestanding','-fms-extensions','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',"-I$inc\km","-I$inc\shared","-I$inc\um")
 foreach($name in @('avstream','frame_codec')){
  & clang @flags "-DA04_HI556_AVSTREAM_EXPERIMENT=$flag" -c "$root\drivers\camera\hi556\$name.c" -o "$out\$name.obj"
  if($LASTEXITCODE){throw "AVStream ARM compile failed: $name"}
 }
 & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\avstream.obj" "$out\frame_codec.obj" "$km\ks.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "$km\libcntpr.lib" "/map:$out\hi556-avstream.map" "/out:$out\package\hi556-avstream.sys"
 if($LASTEXITCODE){throw 'AVStream ARM link failed'}
 Copy-Item "$root\drivers\camera\hi556\hi556-avstream.inf" "$out\package"
 if($CertificateThumbprint){
  & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$out\package\hi556-avstream.sys"
  if($LASTEXITCODE){throw 'AVStream SYS signing failed'}
 }
 & "$kit\bin\x86\Inf2Cat.exe" "/driver:$out\package" /os:6_3_ARM /uselocaltime
 if($LASTEXITCODE){throw 'AVStream Inf2Cat failed'}
 if($CertificateThumbprint){
  & "$kit\bin\x86\signtool.exe" sign /fd SHA256 /s My /sha1 $CertificateThumbprint "$out\package\hi556-avstream.cat"
  if($LASTEXITCODE){throw 'AVStream CAT signing failed'}
  foreach($name in @('hi556-avstream.sys','hi556-avstream.inf')){
   & "$kit\bin\x86\signtool.exe" verify /pa /v /c "$out\package\hi556-avstream.cat" "$out\package\$name"
   if($LASTEXITCODE){throw "AVStream catalog membership failed: $name"}
  }
 }
} finally {$env:TEMP=$priorTemp;$env:TMP=$priorTmp;$env:RUNNER_TEMP=$priorRunner}
[ordered]@{
 status='avstream-capture-candidate-not-hardware-validated';source_commit=$commit;source_dirty=$false
 version='0.0.1.0';recipe_sha256=(Hash $PSCommandPath);critical_flags=@{A04_HI556_AVSTREAM_EXPERIMENT=$flag}
 required_raw_owner_version='0.0.1.3';raw_status_schema=3;model='rear HI556 chip0556'
 avstream_api='WDK8.1 KsInitializeDriver and pin-centric PASSIVE processing'
 geometry=@{width=648;height=486;stride=1944;format='bottom-up RGB24';maximum_software_fps=1;hardware_fps_measured=$false}
 raw_packing='MT8167 little-endian continuous10bit, GBRG; average actual2x2 green; no WB/color calibration'
 separate_i2c_mmio_owner=$false;spm_infra_map=$false;synthetic_fallback=$false;capture_on_load=$false
 default_enabled=$false;firmware_or_mma_rebuilt=$false
 signed=[bool]$CertificateThumbprint;certificate_thumbprint=$CertificateThumbprint;hardware_verified=$false;winrt_app_verified=$false
 blockers=@('Real HI556 NCSI2-to-MUX nonzero frame still unproven; no inferred PHY/MUX repair','Requires matched opt-in board RAW owner/NVS/ACPI firmware and MMA2.1 shared owner','No front sensor, 3A/OTP/color calibration/continuous hardware ring; initial bounded single-shot capture backend')
 files=@(Get-ChildItem "$out\package" -File|ForEach-Object{@{path=('package/'+$_.Name);bytes=$_.Length;sha256=(Hash $_.FullName)}})
}|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 "$out\manifest.json"
