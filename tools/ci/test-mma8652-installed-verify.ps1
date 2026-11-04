# Tests control flow in a private fixture; no actual registry, device or service access.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$t=if ($env:RUNNER_TEMP) {$env:RUNNER_TEMP} else {$env:TEMP}
$b=Join-Path $t ('mma-verify-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $b | Out-Null
try {
 $mock=@'
@echo off
if /i "%1"=="minint" if exist "%A04_VERIFY_BASE%\winpe" exit /b 0
if /i "%1"=="minint" exit /b 1
if exist "%A04_VERIFY_BASE%\fail-%1" exit /b 2
exit /b 0
'@
 [IO.File]::WriteAllText("$b\control.cmd",$mock,[Text.Encoding]::ASCII)
 $script=Get-Content (Join-Path $root 'tools\winpe\verify-mma8652-installed.cmd') -Raw
 $script=$script.Replace('reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT',"call `"$b\control.cmd`" minint")
 $script=$script.Replace('reg query HKLM\SYSTEM\CurrentControlSet\Services\A04Mma8652',"call `"$b\control.cmd`" service")
 $script=$script.Replace('sc query A04Mma8652',"call `"$b\control.cmd`" service")
 $script=$script.Replace('"%~dp0mma8652-status.exe"',"call `"$b\control.cmd`" pnp")
 $script=$script.Replace('"%~dp0mma8652-read.exe"',"call `"$b\control.cmd`" read")
 [IO.File]::WriteAllText("$b\verify.cmd",$script,[Text.Encoding]::ASCII)
 $env:A04_VERIFY_BASE=$b
 function Check([string]$expected,[int]$code) {
   $out=& cmd.exe /c "$b\verify.cmd" 2>&1 | Out-String
   if ($LASTEXITCODE -ne $code -or $out -notmatch $expected) { throw "Unexpected: $out exit $LASTEXITCODE" }
 }
 Check 'MMA8652_INSTALLED_WINDOWS_RAW_HID_OBSERVED' 0
 New-Item "$b\winpe" -ItemType File | Out-Null; Check 'MMA8652_VERIFY_REJECTED_WINPE' 2; Remove-Item "$b\winpe"
 New-Item "$b\fail-service" -ItemType File | Out-Null; Check 'MMA8652_INSTALLED_SERVICE_MISSING' 3; Remove-Item "$b\fail-service"
 New-Item "$b\fail-pnp" -ItemType File | Out-Null; Check 'MMA8652_INSTALLED_PNP_FAILED' 4; Remove-Item "$b\fail-pnp"
 New-Item "$b\fail-read" -ItemType File | Out-Null; Check 'MMA8652_INSTALLED_HID_READ_FAILED' 5
 Write-Output 'MMA8652_INSTALLED_VERIFY_MOCK_TESTS_OK'
} finally {
 Remove-Item Env:A04_VERIFY_BASE -ErrorAction SilentlyContinue
 Remove-Item -LiteralPath $b -Recurse -Force
}
