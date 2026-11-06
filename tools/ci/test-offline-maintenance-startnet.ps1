$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP required' }
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$dir=Join-Path $env:RUNNER_TEMP 'offline-maintenance-startnet-fixture'
if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$src=Get-Content -Raw (Join-Path $root 'tools\winpe\startnet-offline-maintenance.cmd')
[IO.File]::WriteAllText((Join-Path $dir 'present'),'fixture')
foreach($case in @('ok','init','driver','esp','journal','badjournal','incomplete')){
 $init=if($case -eq 'init'){1}else{0};$drv=if($case -eq 'driver'){1}else{0};$mount=if($case -eq 'esp'){1}else{0}
 $s=$src -replace '(?m)^wpeinit\r?$', "cmd /c exit $init"
 $s=$s -replace '(?m)^drvload .*\r?$', "cmd /c exit $drv"
 $s=$s -replace '(?m)^if exist E:.*goto ready\r?$', 'if exist "never-present" goto ready'
 $s=$s -replace '(?m)^mountvol E: /s\r?$', "cmd /c exit $mount"
 $ready=if($case -in @('journal','badjournal')){'present'}else{'missing'}
 $incomplete=if($case -eq 'incomplete'){'present'}else{'missing'}
 $probe=if($case -eq 'badjournal'){1}else{0}
 $s=$s.Replace('if exist E:\A04BACKUP\READY goto journal',"if exist `"$dir\$ready`" goto journal")
 $s=$s.Replace('if exist E:\A04BACKUP goto incomplete',"if exist `"$dir\$incomplete`" goto incomplete")
 $s=$s -replace '(?m)^X:\\a04br3\\offline-backup.exe --probe\r?$', "cmd /c exit $probe"
 $s=$s -replace '(?m)^cmd.exe /k\r?$', 'echo fixture-shell'
 $file=Join-Path $dir "$case.cmd";[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 $lines=& cmd.exe /c $file
 switch($case){
  'ok' {if(!($lines -match 'OFFLINE_MAINTENANCE_READY_NO_UPDATE')){throw 'ready flow missing'}}
  'journal' {if(!($lines -match 'OFFLINE_RECOVERY_JOURNAL_AVAILABLE_NO_UPDATE')){throw 'journal flow missing'}}
  'badjournal' {if(!($lines -match 'OFFLINE_RECOVERY_JOURNAL_UNVERIFIED_NO_UPDATE')){throw 'invalid journal mismatch'}}
  'incomplete' {if(!($lines -match 'OFFLINE_RECOVERY_JOURNAL_UNVERIFIED_NO_UPDATE')){throw 'incomplete journal mismatch'}}
  'init' {if(!($lines -match 'OFFLINE_MAINTENANCE_INIT_FAILED') -or ($lines -match 'OFFLINE_MAINTENANCE_READY_NO_UPDATE')){throw 'init failure mismatch'}}
  'driver' {if(!($lines -match 'OFFLINE_MAINTENANCE_STORAGE_FAILED') -or ($lines -match 'OFFLINE_MAINTENANCE_READY_NO_UPDATE')){throw 'driver failure mismatch'}}
  'esp' {if(!($lines -match 'OFFLINE_MAINTENANCE_ESP_MOUNT_FAILED') -or ($lines -match 'OFFLINE_MAINTENANCE_READY_NO_UPDATE')){throw 'ESP failure mismatch'}}
 }
}
Write-Host 'OFFLINE_MAINTENANCE_STARTNET_PASS'
