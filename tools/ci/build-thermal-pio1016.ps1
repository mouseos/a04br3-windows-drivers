# Fixed parent1016 + thermal0.0.1.4 only. Pair with board PIO handoff boot.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $root 'build\thermal-pio1016'
if(Test-Path $out){throw 'Use fresh output'}
if(@(& git -C $root status --porcelain).Count){throw 'Commit source before build'}
$commit=(& git -C $root rev-parse HEAD)
$baseline=Get-Content (Join-Path $root 'drivers\power\parent1008-baseline-flags.json') -Raw|ConvertFrom-Json
$values=@{RUNNER_TEMP=$out;A04_POWER_CANDIDATE_ROOT=$out;A04_THERMAL_NATIVE_PARENT='1';A04_THERMAL_SHARED_PARENT='0';A04_THERMAL_ONESHOT_TEST='0';A04_THERMAL_MONITOR_TEST='0';A04_THERMAL_LEASE_DIAG='0';A04_THERMAL_CODEC_ADC_DIAGNOSTIC='0'}
foreach($p in $baseline.flags.PSObject.Properties){$values[$p.Name]=[string]$p.Value}
$values.A04_SHARED_THERMAL_DIAGNOSTIC='1';$values.A04_SHARED_THERMAL_NATIVE_EXPERIMENT='1';$values.A04_SHARED_SD_POWER='0'
$previous=@{};foreach($k in @($values.Keys)+@('TEMP','TMP')){$previous[$k]=[Environment]::GetEnvironmentVariable($k,'Process')}
New-Item -ItemType Directory "$out\tmp" -Force|Out-Null
try {
 $env:TEMP="$out\tmp";$env:TMP=$env:TEMP
 foreach($k in $values.Keys){[Environment]::SetEnvironmentVariable($k,$values[$k],'Process')}
 & (Join-Path $PSScriptRoot 'build-msdc-parent-probe.ps1')
 & (Join-Path $PSScriptRoot 'build-mt8167-thermal.ps1')
 $p=Join-Path $out 'parent';New-Item -ItemType Directory $p|Out-Null
 foreach($n in @('mt8167-shared.sys','shared-parent.inf','parent-manifest.json')){Copy-Item (Join-Path $out "a04-msdc-parent-probe\$n") $p}
 $t=Join-Path $out 'mt8167-thermal';$packages=@($p,$t)
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$sign=Join-Path $kit 'bin\x86\signtool.exe'
 foreach($d in $packages){
  & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My "$d\$(if($d -eq $p){'mt8167-shared.sys'}else{'mt8167-thermal.sys'})"
  if($LASTEXITCODE){throw 'SYS signing failed'}
  Remove-Item "$d\*.cat" -ErrorAction SilentlyContinue
  & (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$d" /os:6_3_ARM
  if($LASTEXITCODE){throw 'Final catalog failed'}
  $cat=@(Get-ChildItem $d -Filter '*.cat');if($cat.Count -ne 1){throw 'Expected one CAT'}
  & $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My $cat[0].FullName
  if($LASTEXITCODE){throw 'CAT signing failed'}
  foreach($f in Get-ChildItem $d -File|Where-Object {$_.Extension -in @('.sys','.cat')}){
   & $sign verify /pa /q $f.FullName
   if($LASTEXITCODE -or (Get-AuthenticodeSignature $f.FullName).SignerCertificate.Thumbprint -ne $CertificateThumbprint){throw 'Signature/signer mismatch'}
  }
  foreach($f in Get-ChildItem $d -File|Where-Object {$_.Extension -in @('.sys','.inf')}){
   & $sign verify /pa /q /c $cat[0].FullName $f.FullName
   if($LASTEXITCODE){throw 'CAT membership mismatch'}
  }
  foreach($f in Get-ChildItem $d -Filter '*.sys'){
   $pe=& llvm-readobj --file-headers $f.FullName
   if($LASTEXITCODE -or ($pe -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){throw 'Not ARMNT'}
  }
 }
 $pmPath=Join-Path $p 'parent-manifest.json';$pm=Get-Content $pmPath -Raw|ConvertFrom-Json
 if($pm.driver_version -ne '2026.11.7.1016'){throw 'Wrong parent version'}
 $pm|Add-Member -NotePropertyName unsigned_sys_sha256 -NotePropertyValue $pm.sys_sha256 -Force
 $pm.sys_sha256=(Get-FileHash "$p\mt8167-shared.sys" -Algorithm SHA256).Hash
 $pm|Add-Member -NotePropertyName signed -NotePropertyValue $true -Force
 $pm|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $pmPath
 @{source_commit=$commit;parent_ver='2026.11.7.1016';thermal_ver='0.0.1.4';signer=$CertificateThumbprint;utc=[DateTime]::UtcNow.ToString('o');flags=$values;device_verified=$false;
  handoff_record_sha256=(Get-FileHash (Join-Path $root 'drivers\bus\mt8167-shared\thermal_pio_record.h') -Algorithm SHA256).Hash;
  files=@(foreach($d in $packages){Get-ChildItem $d -File|Where-Object {$_.Extension -in @('.sys','.inf','.cat','.json')}|ForEach-Object {@{path=$_.FullName.Substring($out.Length+1);bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
 }|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 "$out\signed-manifest.json"
}finally{foreach($k in $previous.Keys){[Environment]::SetEnvironmentVariable($k,$previous[$k],'Process')}}
