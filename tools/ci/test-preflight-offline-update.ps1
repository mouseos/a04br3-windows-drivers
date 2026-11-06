$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dir=Join-Path $env:RUNNER_TEMP 'offline-preflight-fixture';if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$original=[IO.File]::ReadAllText("$root\tools\winpe\preflight-offline-update.cmd")
foreach($case in @('ok','baseline','copy','load','pnp','unload','payload','sign')){
 $s=$original;$trace=Join-Path $dir ($case+'.trace')
 foreach($p in @(@('X:\a04br3\offline-baseline-guard.exe','baseline'),@('X:\a04br3\audit-copy-hives.exe','copy'),@('reg load HKLM\A04AUDITSYS X:\a04br3\audit-system\SYSTEM','load'),@('X:\a04br3\offline-pnp-baseline.exe','pnp'),@('reg unload HKLM\A04AUDITSYS','unload'),@('X:\a04br3\offline-hive-apply.exe --payload-check','payload'),@('X:\a04br3\offline-signature-check.exe','sign'))){
  $code=[int]($case -eq $p[1]);$replacement="echo $($p[1])>>`"$trace`"`r`ncmd /c exit $code";$s=$s.Replace($p[0],$replacement)
 }
 $file=Join-Path $dir ($case+'.cmd');[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 $lines=& cmd.exe /c $file;$rc=$LASTEXITCODE
 $expected=switch($case){'ok'{0}'baseline'{2}'copy'{3}'load'{4}'pnp'{6}'unload'{5}'payload'{7}'sign'{8}}
 if($rc -ne $expected){throw "preflight $case returned $rc expected $expected"}
 $events=Get-Content $trace
 if($case -eq 'pnp' -and !($events -contains 'unload')){throw 'PnP failure skipped hive unload'}
 if($case -in @('baseline','copy','load','pnp','unload') -and ($events -contains 'payload')){throw 'checks continued after failure'}
 if($case -ne 'ok' -and ($lines -match 'UPDATE_PREFLIGHT_PASSED')){throw 'failure was called success'}
}
Write-Host 'OFFLINE_PREFLIGHT_CONTROL_FLOW_PASS (helpers mocked; no update)'
