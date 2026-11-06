$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dir=Join-Path $env:RUNNER_TEMP 'audit-once-fixture'
if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$src=[IO.File]::ReadAllText("$root\tools\winpe\run-audit-once.cmd").Replace('X:\a04br3',$dir)
foreach($case in @('ok','guard','copy','load','pnp','unload','release','uart')){
 $g=[int]($case -eq 'guard');$c=if($case -eq 'copy'){4}else{0};$ld=[int]($case -eq 'load');$p=[int]($case -eq 'pnp');$u=[int]($case -eq 'unload');$r=[int]($case -eq 'release');$t=[int]($case -eq 'uart')
 $s=$src
 foreach($pair in @(@('guard-windows-volume.exe',$g),@('audit-volume-sys.exe',0),@('audit-copy-hives.exe',$c),@('audit-pnp-inf.exe',$p),@('release-offline-volume.exe',$r))){$escaped=[regex]::Escape((Join-Path $dir $pair[0]));$s=$s -replace "(?m)^$escaped (>>.*)$",("cmd /c exit $($pair[1]) `$1")}
 $s=$s -replace '(?m)^reg load HKLM\\A04AUDITSYS .* (>>.*)$',"cmd /c exit $ld `$1"
 $s=$s -replace '(?m)^reg unload HKLM\\A04AUDITSYS (>>.*)$',"cmd /c exit $u `$1"
 $sender=[regex]::Escape((Join-Path $dir 'uart-audit-stream.exe'));$s=$s -replace "(?m)^$sender .*", "cmd /c exit $t"
 $file=Join-Path $dir "$case.cmd";[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 & cmd.exe /c $file | Out-Null;$rc=$LASTEXITCODE
 $expected=switch($case){'ok'{10}'guard'{9}'copy'{3}'load'{4}'pnp'{6}'unload'{5}'release'{9}'uart'{8}}
 if($rc -ne $expected){throw "case $case exit $rc expected $expected"}
 $report=Get-Content -Raw (Join-Path $dir 'toprgu-audit-result.txt')
 if($case -eq 'guard' -and ($report -notmatch 'IDENTITY_REJECTED_NO_RELEASE' -or $report -match 'AUDIT_RELEASE_EXIT=')){throw 'guard failure must not release'}
 if($case -eq 'copy' -and ($report -notmatch 'AUDIT_HIVE_COPY_EXIT=4' -or $report -notmatch 'AUDIT_STATUS=HIVE_COPY_REJECTED' -or $report -notmatch 'AUDIT_RELEASE_EXIT=0')){throw 'copy failure not reported and released'}
}
Write-Host 'AUDIT_ONCE_CONTROL_FLOW_FIXTURE_PASS (helpers mocked)'
$global:LASTEXITCODE=0
