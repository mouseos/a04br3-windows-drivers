$ErrorActionPreference='Stop'
if(!$env:RUNNER_TEMP){throw 'RUNNER_TEMP required'}
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$dir=Join-Path $env:RUNNER_TEMP 'offline-hive-transaction-fixture'
if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$src=(Get-Content -Raw (Join-Path $root 'tools\winpe\apply-offline-hives.cmd')).Replace('X:\a04br3',$dir)
foreach($name in @('offline-backup.exe','offline-hive-apply.exe','release-offline-volume.exe')){[IO.File]::WriteAllText((Join-Path $dir $name),'fixture')}
$backup=[regex]::Escape((Join-Path $dir 'offline-backup.exe'));$hive=[regex]::Escape((Join-Path $dir 'offline-hive-apply.exe'));$release=[regex]::Escape((Join-Path $dir 'release-offline-volume.exe'))
foreach($case in @('ok','check','backup','probe','arm','apply','restore','release','complete')){
 $check=if($case -eq 'check'){1}else{0};$save=if($case -eq 'backup'){1}else{0};$probe=if($case -eq 'probe'){1}else{0};$arm=if($case -eq 'arm'){1}else{0};$apply=if($case -in @('apply','restore')){1}else{0};$restore=if($case -eq 'restore'){1}else{0};$rel=if($case -eq 'release'){1}else{0};$complete=if($case -eq 'complete'){1}else{0}
 $s=$src -replace '(?m)^if exist E:\\A04BACKUP goto existing\r?$', 'if exist "never-present" goto existing'
 $s=$s -replace "(?m)^$hive --check\r?$", "cmd /c exit $check"
 $s=$s -replace "(?m)^$backup\r?$", "cmd /c exit $save"
 $s=$s -replace "(?m)^$backup --probe\r?$", "cmd /c exit $probe"
 $s=$s -replace "(?m)^$backup --arm\r?$", "cmd /c exit $arm"
 $s=$s -replace "(?m)^$hive --apply\r?$", "cmd /c exit $apply"
 $s=$s -replace "(?m)^$backup --restore\r?$", "cmd /c exit $restore"
 $s=$s -replace "(?m)^$backup --complete\r?$", "cmd /c exit $complete"
 $s=$s -replace "(?m)^$release\r?$", "cmd /c exit $rel"
 $file=Join-Path $dir "$case.cmd";[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII);$lines=& cmd.exe /c $file;$rc=$LASTEXITCODE
 $expect=switch($case){'ok'{0}'check'{2}'backup'{2}'probe'{4}'arm'{4}'apply'{3}'restore'{4}'release'{5}'complete'{6}}
 if($rc -ne $expect){throw "transaction case $case returned $rc instead of $expect"}
 if($case -eq 'ok' -and !($lines -match 'HIVE_TRANSACTION_APPLIED_DRIVER_PNP_UNVERIFIED')){throw 'success marker missing'}
 if($case -eq 'apply' -and !($lines -match 'HIVE_TRANSACTION_ROLLED_BACK')){throw 'rollback marker missing'}
 if($case -eq 'restore' -and !($lines -match 'HIVE_TRANSACTION_RECOVERY_INCOMPLETE_RETAIN_JOURNAL')){throw 'failed recovery marker missing'}
}
Write-Host 'OFFLINE_HIVE_TRANSACTION_FIXTURE_PASS'
$global:LASTEXITCODE=0
