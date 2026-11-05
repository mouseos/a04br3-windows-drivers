$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..');$base=Join-Path $env:RUNNER_TEMP 'a04-cpu-startnet-test'
if(Test-Path $base){throw 'test directory exists'}
New-Item -ItemType Directory -Path $base | Out-Null
$token='0123456789ABCDEF'
[IO.File]::WriteAllText("$base\audit.cmd", "@echo off`r`necho TOPRGU_AUDIT_FINISHED_READ_ONLY`r`nexit /b 0`r`n",[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$base\report.cmd", "@echo off`r`necho REPORT_TOKEN=%1`r`nexit /b 0`r`n",[Text.Encoding]::ASCII)
function MakeTest([int]$storage) {
 $s=Get-Content (Join-Path $root 'tools\winpe\startnet-cpu-audit.cmd.in') -Raw
 $s=$s.Replace('X:\a04br3',"$base").Replace('@TOKEN@',$token)
 $s=$s -replace '(?m)^wpeinit\r?$', 'cmd /c exit 0'
 $s=$s -replace '(?m)^drvload .*\r?$', "cmd /c exit $storage"
 $s=$s -replace '(?m)^call .*\\audit-toprgu-installed\.cmd ', "call $base\audit.cmd "
 $s=$s -replace '(?m)^.*\\uart-cpu-report\.exe ', "call $base\report.cmd "
 return $s
}
[IO.File]::WriteAllText("$base\success.cmd",(MakeTest 0),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\success.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'TOPRGU_AUDIT_FINISHED_READ_ONLY' -or $report -notmatch 'TOPRGU_AUDIT_EXIT=0'){throw 'success flow failed'}
Remove-Item "$base\toprgu-audit-result.txt"
[IO.File]::WriteAllText("$base\fail.cmd",(MakeTest 1),[Text.Encoding]::ASCII)
$out=& cmd.exe /c "$base\fail.cmd"
$report=Get-Content "$base\toprgu-audit-result.txt" -Raw
if(!($out -match "REPORT_TOKEN=$token") -or $report -notmatch 'TOPRGU_AUDIT_STORAGE_FAILED' -or $report -notmatch 'TOPRGU_AUDIT_EXIT=2' -or $report -match 'FINISHED_READ_ONLY'){throw 'storage failure flow failed'}
Write-Output 'CPU_AUDIT_STARTNET_FIXTURE_OK'
$global:LASTEXITCODE=0
