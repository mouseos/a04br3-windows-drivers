$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$base=Join-Path $env:RUNNER_TEMP 'a04-cpu-startnet-test'
if(Test-Path $base){throw 'test directory exists'}
New-Item -ItemType Directory -Path $base | Out-Null
$token='0123456789ABCDEF'
[IO.File]::WriteAllText("$base\audit.cmd", "@echo off`r`necho TOPRGU_AUDIT_FINISHED_READ_ONLY`r`nexit /b 0`r`n",[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$base\report.cmd", "@echo off`r`necho REPORT_TOKEN=%1`r`nexit /b 0`r`n",[Text.Encoding]::ASCII)
function MakeTest([int]$storage, [int]$mount=0) {
 $s=Get-Content (Join-Path $root 'tools\winpe\startnet-cpu-audit.cmd.in') -Raw
 $s=$s.Replace('X:\a04br3',"$base").Replace('@TOKEN@',$token)
 $s=$s -replace '(?m)^wpeinit\r?$', 'cmd /c exit 0'
 $s=$s -replace '(?m)^drvload .*\r?$', "cmd /c exit $storage"
 $s=$s -replace '(?m)^mountvol C: /p (.*)$', ("cmd /c exit $mount " + '$1')
 $s=$s -replace '(?m)^call .*\\audit-toprgu-installed\.cmd ', "call $base\audit.cmd "
 $s=$s -replace '(?m)^.*\\uart-cpu-report\.exe ', "call $base\report.cmd "
 return $s
}
[IO.File]::WriteAllText("$base\success.cmd",(MakeTest 0),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\success.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'TOPRGU_AUDIT_FINISHED_READ_ONLY' -or $report -notmatch 'TOPRGU_AUDIT_EXIT=0' -or $report -notmatch 'MOUNTVOL_EXIT=0'){throw 'success flow failed'}
Remove-Item "$base\toprgu-audit-result.txt"
[IO.File]::WriteAllText("$base\fail.cmd",(MakeTest 1),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\fail.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'TOPRGU_AUDIT_STORAGE_FAILED' -or $report -notmatch 'TOPRGU_AUDIT_EXIT=2' -or $report -match 'FINISHED_READ_ONLY'){throw 'storage failure flow failed'}
Remove-Item "$base\toprgu-audit-result.txt"
[IO.File]::WriteAllText("$base\mountfail.cmd",(MakeTest 0 5),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\mountfail.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'MOUNTVOL_EXIT=5'){throw 'dismount failure not reported'}
Remove-Item "$base\toprgu-audit-result.txt"
[IO.File]::WriteAllText("$base\audit.cmd", "@echo off`r`necho TOPRGU_AUDIT_VOLUME_REJECTED`r`nexit /b 2`r`n",[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$base\guardfail.cmd",(MakeTest 0),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\guardfail.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'TOPRGU_AUDIT_VOLUME_REJECTED' -or $report -notmatch 'TOPRGU_AUDIT_EXIT=2' -or $report -match 'MOUNTVOL_EXIT='){throw 'guard rejection did not prevent dismount'}
Write-Output 'CPU_AUDIT_STARTNET_FIXTURE_OK'
$global:LASTEXITCODE=0
