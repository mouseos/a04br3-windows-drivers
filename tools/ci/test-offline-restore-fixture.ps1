$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP required' }
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$dir=Join-Path $env:RUNNER_TEMP 'offline-restore-fixture'
if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$source=(Get-Content -Raw (Join-Path $root 'tools\winpe\restore-offline-backup.cmd')).Replace('X:\a04br3',$dir)
foreach($name in @('offline-backup.exe','release-offline-volume.exe')){[IO.File]::WriteAllText((Join-Path $dir $name),'fixture')}
$restore=[regex]::Escape((Join-Path $dir 'offline-backup.exe'))
$release=[regex]::Escape((Join-Path $dir 'release-offline-volume.exe'))
foreach($case in @('ok','restore','release')){
 $r=if($case -eq 'restore'){1}else{0};$d=if($case -eq 'release'){1}else{0}
 $s=$source -replace "(?m)^$restore --restore\r?$", "cmd /c exit $r"
 $s=$s -replace "(?m)^$release\r?$", "cmd /c exit $d"
 $file=Join-Path $dir "$case.cmd";[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 $lines=& cmd.exe /c $file;$rc=$LASTEXITCODE
 if($case -eq 'ok' -and ($rc -ne 0 -or !($lines -match 'OFFLINE_RECOVERY_RELEASED_NTFS_INTEGRITY_UNVERIFIED'))){throw 'success flow mismatch'}
 if($case -eq 'restore' -and ($rc -ne 3 -or ($lines -match 'OFFLINE_RECOVERY_RELEASED'))){throw 'restore failure mismatch'}
 if($case -eq 'release' -and ($rc -ne 4 -or ($lines -match 'OFFLINE_RECOVERY_RELEASED'))){throw 'release failure mismatch'}
}
Write-Host 'OFFLINE_RESTORE_FIXTURE_PASS'
