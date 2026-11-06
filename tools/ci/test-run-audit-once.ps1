$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dir=Join-Path $env:RUNNER_TEMP 'audit-once-fixture'
if(Test-Path $dir){throw 'fixture exists'}
New-Item -ItemType Directory $dir | Out-Null
$src=[IO.File]::ReadAllText("$root\tools\winpe\run-audit-once.cmd").Replace('X:\a04br3',$dir)
foreach($case in @('ok','load-driver','pin','map','notfound','guard','copy','load','pnp','unload','release','uart')){
 $map=if($case -eq 'map'){1}elseif($case -eq 'notfound'){4}else{0};$g=[int]($case -eq 'guard');$observe=if($case -in @('load-driver','pin')){4}else{0};$pin=if($case -eq 'pin'){2}else{0};$c=if($case -eq 'copy'){4}else{0};$ld=[int]($case -eq 'load');$p=[int]($case -eq 'pnp');$u=[int]($case -eq 'unload');$r=[int]($case -eq 'release');$t=[int]($case -eq 'uart')
 $s=$src
 foreach($pair in @(@('map-offline-windows.exe',$map),@('guard-windows-volume.exe',$g),@('audit-volume-sys.exe',0),@('audit-copy-hives.exe',$c),@('audit-pnp-inf.exe',$p),@('release-offline-volume.exe',$r))){$escaped=[regex]::Escape((Join-Path $dir $pair[0]));$s=$s -replace "(?m)^$escaped (?:--wait |--verify-package )?(>>.*)$",("cmd /c exit $($pair[1]) `$1")}
 $obs=[regex]::Escape((Join-Path $dir 'observe-msdc-storage.exe'))
 $s=$s -replace "(?m)^$obs --verify-package (>>.*)$",("cmd /c exit $pin `$1")
 $s=$s -replace "(?m)^$obs (?:--wait )?(>>.*)$",("cmd /c exit $observe `$1")
 $s=$s -replace '(?m)^drvload .* (>>.*)$', 'cmd /c exit 0 $1'
 $s=$s -replace '(?m)^reg load HKLM\\A04AUDITSYS .* (>>.*)$',"cmd /c exit $ld `$1"
 $s=$s -replace '(?m)^reg unload HKLM\\A04AUDITSYS (>>.*)$',"cmd /c exit $u `$1"
 $sender=[regex]::Escape((Join-Path $dir 'uart-audit-stream.exe'));$s=$s -replace "(?m)^$sender .*", "cmd /c exit $t"
 $file=Join-Path $dir "$case.cmd";[IO.File]::WriteAllText($file,$s,[Text.Encoding]::ASCII)
 & cmd.exe /c $file | Out-Null;$rc=$LASTEXITCODE
 $expected=switch($case){'ok'{10}'load-driver'{10}'pin'{9}'map'{9}'notfound'{9}'guard'{9}'copy'{3}'load'{4}'pnp'{6}'unload'{5}'release'{9}'uart'{8}}
 if($rc -ne $expected){throw "case $case exit $rc expected $expected"}
 $report=Get-Content -Raw (Join-Path $dir 'toprgu-audit-result.txt')
 if($case -eq 'load-driver' -and ($report -notmatch 'MSDC_OBSERVE_PRE_EXIT=4' -or $report -notmatch 'MSDC_DRVLOAD_EXIT=0')){throw 'drvload path not exercised'}
 if($case -eq 'pin' -and ($report -notmatch 'MSDC_PACKAGE_PIN_REJECTED' -or $report -match 'MSDC_DRVLOAD_EXIT=')){throw 'bad pin not rejected before drvload'}
 if($case -eq 'guard' -and ($report -notmatch 'IDENTITY_REJECTED_NO_RELEASE' -or $report -match 'AUDIT_RELEASE_EXIT=')){throw 'guard failure must not release'}
 if($case -eq 'notfound' -and ($report -notmatch 'A04MAP_ATTEMPT=6' -or $report -notmatch 'A04MAP_EXIT=4')){throw 'bounded enumeration retry missing'}
 if($case -eq 'copy' -and ($report -notmatch 'AUDIT_HIVE_COPY_EXIT=4' -or $report -notmatch 'AUDIT_STATUS=HIVE_COPY_REJECTED' -or $report -notmatch 'AUDIT_RELEASE_EXIT=0')){throw 'copy failure not reported and released'}
}
Write-Host 'AUDIT_ONCE_CONTROL_FLOW_FIXTURE_PASS (helpers mocked)'
$global:LASTEXITCODE=0
