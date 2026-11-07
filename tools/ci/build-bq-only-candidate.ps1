# Fixed BQ/gauge-only WDK8.1 ARM candidate; no parent or thermal build/deploy.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$CertificateThumbprint,
 [ValidateSet('Observe','ColdLinux100','ColdLinux100D0')][string]$ChargerProfile='Observe')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $root 'build\bq-only-candidate'
if(Test-Path $out){throw 'Use fresh output'}
if(@(& git -C $root status --porcelain).Count){throw 'Commit source before build'}
$commit=(& git -C $root rev-parse HEAD)
$values=@{A04_POWER_CANDIDATE_ROOT=$out;RUNNER_TEMP=$out;
 A04_LC709203F_TRACE='1';A04_BQHR_DIAGNOSTIC='1';A04_BQ_CLOCK_DIAGNOSTIC='0';
 A04_BQ24160_EXPERIMENTAL='0';A04_BROKER_OWNER_EXPERIMENTAL='0';A04_BROKER_CLIENT_EXPERIMENTAL='0';
 A04_BQ_DEFAULT_EXIT_EXPERIMENTAL='0';A04_BQ_STOPPED_GAUGE_DIAGNOSTIC='0';
 A04_BQ_D0_DEFAULT_DIAGNOSTIC='0';A04_BQ_COLD_DIAGNOSTIC='0';A04_BQ_LINUX_PROFILE_BOOT='0'}
if($ChargerProfile -in @('ColdLinux100','ColdLinux100D0')) {
 foreach($k in @('A04_BQ24160_EXPERIMENTAL','A04_BROKER_OWNER_EXPERIMENTAL','A04_BQ_DEFAULT_EXIT_EXPERIMENTAL','A04_BQ_COLD_DIAGNOSTIC','A04_BQ_LINUX_PROFILE_BOOT')){$values[$k]='1'}
}
if($ChargerProfile -eq 'ColdLinux100D0'){$values['A04_BQ_D0_DEFAULT_DIAGNOSTIC']='1'}
$previous=@{};foreach($k in @($values.Keys)+@('TEMP','TMP')){$previous[$k]=[Environment]::GetEnvironmentVariable($k,'Process')}
New-Item -ItemType Directory "$out\tmp" -Force|Out-Null
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($k in $values.Keys){[Environment]::SetEnvironmentVariable($k,$values[$k],'Process')}
 & (Join-Path $PSScriptRoot 'build-lc709203f-probe.ps1')
 $p=Join-Path $out 'lc709203f\package';$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
 $sign=Join-Path $kit 'bin\x86\signtool.exe'
 foreach($f in Get-ChildItem $p -File|Where-Object {$_.Extension -in @('.sys','.exe')}){
  & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $f.FullName
  if($LASTEXITCODE){throw 'SYS/EXE sign failed'}
 }
 Remove-Item "$p\*.cat"
 & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$p" /os:6_3_ARM
 if($LASTEXITCODE){throw 'Final catalog failed'}
 & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My "$p\lc709203f.cat"
 if($LASTEXITCODE){throw 'CAT sign failed'}
 foreach($f in Get-ChildItem $p -File|Where-Object {$_.Extension -in @('.sys','.cat','.exe')}){
  & $sign verify /pa /q $f.FullName
  if($LASTEXITCODE -or (Get-AuthenticodeSignature $f.FullName).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signature/signer mismatch'}
 }
 foreach($n in @('lc709203f.sys','lc709203f.inf')){
  & $sign verify /pa /q /c "$p\lc709203f.cat" "$p\$n"
  if($LASTEXITCODE){throw 'CAT membership mismatch'}
 }
 $pe=& llvm-readobj --file-headers "$p\lc709203f.sys"
 if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'Not ARMNT'}
 @{source_commit=$commit;profile=$ChargerProfile;flags=$values;driver_ver='11/07/2026,0.0.1.4';
  signer=$CertificateThumbprint;utc=[DateTime]::UtcNow.ToString('o');
  thermal_included=$false;parent_included=$false;device_verified=$false;
  d0_restart_local_only=($ChargerProfile -eq 'ColdLinux100D0');
  reset_evidence='bus-write-complete-only-no-readback';external_successor_implemented=$false;
  per_device_data_embedded=$false;charge_current_measured=$false;
  files=@(Get-ChildItem $p -File|ForEach-Object {@{path=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
} finally {foreach($k in $previous.Keys){[Environment]::SetEnvironmentVariable($k,$previous[$k],'Process')}}
