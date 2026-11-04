# Executes only against a freshly created sandbox; DISM, registry and device I/O are mocked.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base=Join-Path $tempRoot ('mma-offline-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base | Out-Null
try {
  $src="$base\src"; $win="$base\Windows"
  New-Item -ItemType Directory -Force "$src\mma8652","$src\mma8652-tested","$win\System32\config","$win\System32\drivers","$win\System32\DriverStore\FileRepository","$win\Temp" | Out-Null
  [IO.File]::WriteAllText("$win\System32\config\SYSTEM",'fake fixture hive')
  [IO.File]::WriteAllText("$win\System32\config\SOFTWARE",'fake fixture software')
  foreach($f in @('mma8652.inf','mma8652.sys','mma8652.cat')) {
    [IO.File]::WriteAllText("$src\mma8652\$f", "fixture-$f")
    Copy-Item "$src\mma8652\$f" "$src\mma8652-tested\$f"
  }
  $mockReg=@'
@echo off
if /i "%1"=="load" exit /b 0
if /i "%1"=="unload" exit /b 0
if /i "%1"=="query" if /i "%3"=="/v" echo Current REG_DWORD 0x1& exit /b 0
if exist "%A04_TEST_BASE%\fail-hivequery" exit /b 2
if exist "%A04_TEST_BASE%\service-exists" echo HKEY_LOCAL_MACHINE\A04TEMP\ControlSet001\Services\A04Mma8652
exit /b 0
'@
  [IO.File]::WriteAllText("$base\mock-reg.cmd",$mockReg,[Text.Encoding]::ASCII)
  $mockCtrl=@'
@echo off
if exist "%A04_TEST_BASE%\fail-%1" exit /b 3
exit /b 0
'@
  [IO.File]::WriteAllText("$base\mock-control.cmd",$mockCtrl,[Text.Encoding]::ASCII)
  $mockDism=@'
@echo off
echo mock dism
if exist "%A04_TEST_BASE%\fail-dism" exit /b 7
mkdir "%A04_TEST_BASE%\Windows\System32\DriverStore\FileRepository\mma8652.inf_fixture" >nul 2>&1
for %%F in (mma8652.inf mma8652.sys mma8652.cat) do copy /b "%A04_TEST_BASE%\src\mma8652\%%F" "%A04_TEST_BASE%\Windows\System32\DriverStore\FileRepository\mma8652.inf_fixture\%%F" >nul
if exist "%A04_TEST_BASE%\bad-store" echo mismatch>>"%A04_TEST_BASE%\Windows\System32\DriverStore\FileRepository\mma8652.inf_fixture\mma8652.sys"
exit /b 0
'@
  [IO.File]::WriteAllText("$base\mock-dism.cmd",$mockDism,[Text.Encoding]::ASCII)
  $batch=Get-Content (Join-Path $root 'tools\winpe\startnet-stage-mma8652.cmd') -Raw
  $batch=$batch.Replace('C:\Windows',$win).Replace('X:\a04br3',$src).Replace('reg ',"call `"$base\mock-reg.cmd`" ")
  $batch=$batch -replace '(?m)^wpeinit\r?$',"call `"$base\mock-control.cmd`" init"
  $batch=$batch -replace '(?m)^drvload .*$',"call `"$base\mock-control.cmd`" provider"
  $batch=$batch.Replace("$src\guard-windows-volume.exe", "call `"$base\mock-control.cmd`" guard")
  $batch=$batch.Replace("$src\check-volume-clean.exe", "call `"$base\mock-control.cmd`" dirty")
  $batch=$batch.Replace("$src\mma8652\mma8652-status.exe", "call `"$base\mock-control.cmd`" status")
  $batch=$batch.Replace("$src\mma8652\mma8652-read.exe", "call `"$base\mock-control.cmd`" read")
  $batch=$batch.Replace("$src\compare-files.exe",'fc /b')
  $batch=$batch.Replace('dism.exe',"call `"$base\mock-dism.cmd`"")
  $batch=$batch.Replace('mountvol C: /p','ver >nul')
  [IO.File]::WriteAllText("$base\stage.cmd",$batch,[Text.Encoding]::ASCII)
  $env:A04_TEST_BASE=$base
  function Reset-Fixture {
    Get-ChildItem "$win\Temp" -Force | Remove-Item -Force -Recurse
    Get-ChildItem "$win\System32\DriverStore\FileRepository" -Force | Remove-Item -Force -Recurse
    Get-ChildItem $base -Filter 'fail-*' | Remove-Item -Force
    foreach($n in @('service-exists','bad-store')) { if(Test-Path "$base\$n") { Remove-Item "$base\$n" } }
  }
  function Run-Case([string]$expected,[bool]$dismExpected) {
    $result=& cmd.exe /c "$base\stage.cmd" 2>&1 | Out-String
    if ($result -notmatch $expected) { throw "Missing $expected : $result" }
    $did=Test-Path "$win\Temp\a04br3-mma8652-dism.log"
    if ($did -ne $dismExpected) { throw "Unexpected DISM execution: $result" }
  }
  Reset-Fixture; Run-Case 'MMA8652_OFFLINE_PACKAGE_VERIFIED' $true
  Reset-Fixture; New-Item "$base\service-exists" -ItemType File | Out-Null; Run-Case 'MMA8652_PRECONDITION_REJECTED' $false
  Reset-Fixture; New-Item "$base\fail-hivequery" -ItemType File | Out-Null; Run-Case 'MMA8652_HIVE_QUERY_FAILED' $false
  Reset-Fixture; New-Item "$base\fail-guard" -ItemType File | Out-Null; Run-Case 'VOLUME_GUARD_REJECTED' $false
  Reset-Fixture; New-Item "$win\Temp\a04br3-mma8652-preinstall" -ItemType Directory | Out-Null; Run-Case 'MMA8652_PRECONDITION_REJECTED' $false
  Reset-Fixture; New-Item "$base\fail-dirty" -ItemType File | Out-Null; Run-Case 'MMA8652_VOLUME_DIRTY_OR_UNKNOWN_NO_DISM' $false
  Reset-Fixture; New-Item "$base\fail-read" -ItemType File | Out-Null; Run-Case 'MMA8652_WINPE_VALIDATION_FAILED' $false
  Reset-Fixture; [IO.File]::WriteAllText("$src\mma8652-tested\mma8652.sys",'mismatch'); Run-Case 'MMA8652_PRECONDITION_REJECTED' $false
  Copy-Item "$src\mma8652\mma8652.sys" "$src\mma8652-tested\mma8652.sys" -Force
  Reset-Fixture; New-Item "$base\fail-dism" -ItemType File | Out-Null; Run-Case 'MMA8652_DISM_FAILED_CHECK_PARTIAL_CHANGES' $true
  Reset-Fixture; New-Item "$base\bad-store" -ItemType File | Out-Null; Run-Case 'MMA8652_STAGING_UNVERIFIED_CHECK_PARTIAL_CHANGES' $true
  Write-Output 'MMA8652_OFFLINE_STAGE_MOCK_TESTS_OK'
} finally {
  Remove-Item Env:A04_TEST_BASE -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $base -Recurse -Force
}
