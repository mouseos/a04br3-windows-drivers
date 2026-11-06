$ErrorActionPreference='Stop'
if (!$env:RUNNER_TEMP) { throw 'RUNNER_TEMP required' }
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$dir=Join-Path $env:RUNNER_TEMP 'toprgu-release-fixture'
if (Test-Path $dir) { throw 'fixture directory already exists' }
New-Item -ItemType Directory $dir | Out-Null
$source=Get-Content -Raw (Join-Path $root 'tools\winpe\release-toprgu-transaction-volume.cmd')
foreach($file in @('guard-windows-volume.exe','flush-toprgu-volume.exe')) {
 [IO.File]::WriteAllText((Join-Path $dir $file),'fixture')
}
$source=$source.Replace('X:\a04br3', $dir)
$guard=[regex]::Escape((Join-Path $dir 'guard-windows-volume.exe'))
$flush=[regex]::Escape((Join-Path $dir 'flush-toprgu-volume.exe'))
foreach($case in @('ok','guard','flush','dismount')) {
 $g=if($case -eq 'guard'){1}else{0}
 $f=if($case -eq 'flush'){1}else{0}
 $m=if($case -eq 'dismount'){1}else{0}
 $s=$source -replace "(?m)^$guard\r?$", "cmd /c exit $g"
 $s=$s -replace "(?m)^$flush\r?$", "cmd /c exit $f"
 $s=$s -replace '(?m)^mountvol C: /p\r?$', "cmd /c exit $m"
 $file=Join-Path $dir "$case.cmd"
 [IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 $lines=& cmd.exe /c $file
 $rc=$LASTEXITCODE
 switch($case) {
  'ok' { if($rc -ne 0 -or !($lines -match 'TOPRGU_TRANSACTION_VOLUME_RELEASED')) {throw 'success incorrectly reported'} }
  'guard' { if($rc -ne 2 -or ($lines -match 'TOPRGU_MOUNTVOL_EXIT')) {throw 'guard failure did not block release'} }
  'flush' { if($rc -ne 3 -or ($lines -match 'TOPRGU_MOUNTVOL_EXIT')) {throw 'flush failure did not block release'} }
  'dismount' { if($rc -ne 4 -or ($lines -match 'TOPRGU_TRANSACTION_VOLUME_RELEASED')) {throw 'dismount failure reported success'} }
 }
}
Write-Output 'TOPRGU_RELEASE_FIXTURE_OK'
