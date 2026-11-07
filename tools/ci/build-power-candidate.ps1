# One fixed WDK8.1 ARM build/sign batch. No deployment or new signing identity.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$CertificateThumbprint,
      [ValidateSet('Observe','ColdLinux100')][string]$ChargerProfile='Observe',
      [ValidateSet('Calibration','SharedOneShot')][string]$ThermalProfile='Calibration')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $root 'build\power-candidate'
if (Test-Path $out) { throw 'Use a fresh checkout/output: build\power-candidate already exists' }
if (@(& git -C $root status --porcelain).Count) { throw 'Commit source before fixed build' }
$commit=(& git -C $root rev-parse HEAD)
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$sign=Join-Path $kit 'bin\x86\signtool.exe'
$cat=Join-Path $kit 'bin\x86\Inf2Cat.exe'
$cert=Get-Item ("Cert:\CurrentUser\My\"+$CertificateThumbprint)
if (!$cert.HasPrivateKey -or $cert.NotAfter.ToUniversalTime() -lt [DateTime]::UtcNow) { throw 'Existing signing identity unavailable or expired' }
$values=@{
 A04_POWER_CANDIDATE_ROOT=$out;
 A04_LC709203F_TRACE='1';A04_BQHR_DIAGNOSTIC='1';A04_BQ_CLOCK_DIAGNOSTIC='0';
 A04_BQ24160_EXPERIMENTAL='0';A04_BROKER_OWNER_EXPERIMENTAL='0';A04_BROKER_CLIENT_EXPERIMENTAL='0';
 A04_BQ_DEFAULT_EXIT_EXPERIMENTAL='0';A04_BQ_STOPPED_GAUGE_DIAGNOSTIC='0';A04_BQ_COLD_DIAGNOSTIC='0';A04_BQ_LINUX_PROFILE_BOOT='0';
 A04_THERMAL_SHARED_PARENT='0';A04_THERMAL_LEASE_DIAG='0';A04_THERMAL_MONITOR_TEST='0';A04_THERMAL_ONESHOT_TEST='0';A04_THERMAL_CODEC_ADC_DIAGNOSTIC='0'
}
if ($ChargerProfile -eq 'ColdLinux100') {
 foreach($key in @('A04_BQ24160_EXPERIMENTAL','A04_BROKER_OWNER_EXPERIMENTAL','A04_BQ_DEFAULT_EXIT_EXPERIMENTAL','A04_BQ_COLD_DIAGNOSTIC','A04_BQ_LINUX_PROFILE_BOOT')) {$values[$key]='1'}
}
if ($ThermalProfile -eq 'SharedOneShot') {
 foreach($key in @('A04_THERMAL_SHARED_PARENT','A04_THERMAL_LEASE_DIAG','A04_THERMAL_MONITOR_TEST','A04_THERMAL_ONESHOT_TEST')) {$values[$key]='1'}
}
$previous=@{};foreach($key in @($values.Keys)+@('TEMP','TMP')) {$previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
New-Item -ItemType Directory $out | Out-Null
try {
 foreach($key in $values.Keys) {[Environment]::SetEnvironmentVariable($key,$values[$key],'Process')}
 & (Join-Path $PSScriptRoot 'build-lc709203f-probe.ps1') *>&1 | Tee-Object (Join-Path $out 'battery-build.log')
 & (Join-Path $PSScriptRoot 'build-mt8167-thermal.ps1') *>&1 | Tee-Object (Join-Path $out 'thermal-build.log')
 $packages=@((Join-Path $out 'lc709203f\package'),(Join-Path $out 'mt8167-thermal'))
 $files=@()
 foreach($package in $packages) {
  foreach($file in Get-ChildItem $package -File | Where-Object {$_.Extension -in @('.sys','.exe')}) {
   & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $file.FullName
   if($LASTEXITCODE -ne 0){throw "Signing failed: $($file.Name)"}
  }
  # Existing CAT hashes are stale after SYS signing; regenerate using real UTC.
  Remove-Item (Join-Path $package '*.cat') -ErrorAction SilentlyContinue
  & $cat "/driver:$package" /os:6_3_ARM
  if($LASTEXITCODE -ne 0){throw 'Final catalog generation failed'}
  foreach($file in Get-ChildItem $package -Filter '*.cat') {
   & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $file.FullName
   if($LASTEXITCODE -ne 0){throw 'Catalog signing failed'}
  }
  foreach($file in Get-ChildItem $package -File | Where-Object {$_.Extension -in @('.sys','.exe','.cat')}) {
   & $sign verify /pa /q $file.FullName
   if($LASTEXITCODE -ne 0){throw "Signature verification failed: $($file.Name)"}
   $signature=Get-AuthenticodeSignature $file.FullName
   if($signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint){throw 'Signer identity mismatch'}
  }
  foreach($file in Get-ChildItem $package -File) {$files+=@{path=$file.FullName.Substring($root.Length+1);sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}}
 }
 $sources=@();foreach($directory in @('drivers\power\bq24160','drivers\power\lc709203f','drivers\power\mt8167-thermal')) {
  foreach($file in Get-ChildItem (Join-Path $root $directory) -File | Where-Object {$_.Extension -in @('.c','.h','.inf')}) {$sources+=@{path=$file.FullName.Substring($root.Length+1);sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}}
 }
 foreach($name in @('build-power-candidate.ps1','build-lc709203f-probe.ps1','build-mt8167-thermal.ps1')) {
  $file=Join-Path $PSScriptRoot $name;$sources+=@{path='tools\ci\'+$name;sha256=(Get-FileHash $file -Algorithm SHA256).Hash}
 }
 @{schema=1;build_utc=[DateTime]::UtcNow.ToString('o');status='signed-source-candidate-not-device-validated';commit=$commit;dirty=$false;
   charger_profile=$ChargerProfile;thermal_profile=$ThermalProfile;flags=$values;
   signer_thumbprint=$cert.Thumbprint;machine='ARMNT';minimum_os='6.3';
   source_hashes=$sources;files=$files;shared_parent_modified=$false;
   thermal_parent_admission_implemented=$false;batovp_hiz_recovery_connected=$false;
   usb_input_limit_ma=100;normal_os_verified=$false;flash_performed=$false;
   kernel_codegen=@('-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none');
   runtime='WDK8.1 Lib/winv6.3/km/arm/libcntpr.lib';
   runtime_sha256=(Get-FileHash (Join-Path $kit 'Lib\winv6.3\km\arm\libcntpr.lib') -Algorithm SHA256).Hash
 } | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $out 'signed-manifest.json')
} finally {foreach($key in $previous.Keys){[Environment]::SetEnvironmentVariable($key,$previous[$key],'Process')}}
