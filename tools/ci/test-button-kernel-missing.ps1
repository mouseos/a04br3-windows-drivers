$ErrorActionPreference='Stop'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base=Join-Path $tempRoot 'a04-kernel-missing-test'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (Test-Path 'HKLM:\A04TEMP') { throw 'A04TEMP exists' }
if (Test-Path 'HKLM:\SOFTWARE\A04KTEST2') { throw 'source exists' }
New-Item -ItemType Directory -Force "$base\Windows\System32\config","$base\Windows\System32\drivers","$base\Windows\Temp","$base\src" | Out-Null
$hive="$base\Windows\System32\config\SYSTEM"
try {
  & reg add HKLM\SOFTWARE\A04KTEST2\Select /v Current /t REG_DWORD /d 1 /f | Out-Null
  & reg add HKLM\SOFTWARE\A04KTEST2\ControlSet001\Services\A04ButtonKernelProbe /v Type /t REG_DWORD /d 1 /f | Out-Null
  & reg add HKLM\SOFTWARE\A04KTEST2\ControlSet001\Services\A04ButtonKernelProbe /v Start /t REG_DWORD /d 1 /f | Out-Null
  & reg add HKLM\SOFTWARE\A04KTEST2\ControlSet001\Services\A04ButtonKernelProbe /v ImagePath /t REG_EXPAND_SZ /d '\SystemRoot\System32\drivers\different.sys' /f | Out-Null
  & reg save HKLM\SOFTWARE\A04KTEST2 $hive /y | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'save failed' }
  & reg delete HKLM\SOFTWARE\A04KTEST2 /f | Out-Null
  $batch=Get-Content (Join-Path $root 'tools\winpe\startnet-remove-button-kernel-probe.cmd') -Raw
  $batch=$batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3\compare-files.exe',"$tempRoot\a04-compare-x64\compare-files.exe").Replace('X:\a04br3',"$base\src")
  $batch=$batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*\r?\nif errorlevel 1 goto driverfail\r?\n','' -replace '(?m)^.*guard-windows-volume.exe\r?\nif errorlevel 1 goto guardfail\r?\n','' -replace '(?m)^mountvol C: /p\r?\n',''
  [IO.File]::WriteAllText("$base\remove.cmd",$batch,[Text.Encoding]::ASCII)
  [IO.File]::WriteAllText("$base\src\flush-probe-reference.txt",'A04BR3-FLUSH-PROBE-v1',[Text.Encoding]::ASCII)
  [IO.File]::WriteAllText("$base\Windows\Temp\a04br3-flush-probe-v1.tmp",'A04BR3-FLUSH-PROBE-v1',[Text.Encoding]::ASCII)
  $out=& cmd.exe /c "$base\remove.cmd"
  if (-not ($out -match 'BUTTON_KERNEL_PROBE_REGISTRY_REJECTED')) { throw 'Wrong image path accepted' }
  if (-not (Test-Path "$base\Windows\Temp\a04br3-flush-probe-v1.tmp")) { throw 'Rejected cleanup removed evidence' }
  & reg load HKLM\A04TEMP $hive | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'load failed' }
  & reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'rejected service missing' }
  & reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v ImagePath /t REG_EXPAND_SZ /d '\SystemRoot\System32\drivers\a04buttonprobe.sys' /f | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'restore path failed' }
  & reg unload HKLM\A04TEMP | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'unload failed' }
  $out=& cmd.exe /c "$base\remove.cmd"
  if (-not ($out -match 'BUTTON_KERNEL_PROBE_REMOVED')) { throw ($out -join ' ') }
  if (-not ($out -match 'BUTTON_FLUSH_PROBE_FILE_REMOVED')) { throw 'Probe file was not cleaned' }
  & reg load HKLM\A04TEMP $hive | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'load failed' }
  & cmd.exe /c 'reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe >nul 2>&1'
  if ($LASTEXITCODE -eq 0) { throw 'service remains' }
  & reg unload HKLM\A04TEMP | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'unload failed' }
  & reg load HKLM\A04TEMP $hive | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'second load failed' }
  & reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Type /t REG_DWORD /d 1 /f | Out-Null
  & reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Start /t REG_DWORD /d 4 /f | Out-Null
  & reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v ImagePath /t REG_EXPAND_SZ /d '\SystemRoot\System32\drivers\a04buttonprobe.sys' /f | Out-Null
  & reg unload HKLM\A04TEMP | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'second unload failed' }
  [IO.File]::WriteAllText("$base\Windows\Temp\a04br3-flush-probe-v1.tmp",'different',[Text.Encoding]::ASCII)
  $out=& cmd.exe /c "$base\remove.cmd"
  if (-not ($out -match 'BUTTON_FLUSH_PROBE_FILE_PRESERVED')) { throw 'Mismatched file was not preserved' }
  if (-not (Test-Path "$base\Windows\Temp\a04br3-flush-probe-v1.tmp")) { throw 'Mismatched file was removed' }
  Write-Output 'MISSING_FILE_REMOVAL_TEST_OK'
} finally {
  & cmd.exe /c 'reg query HKLM\A04TEMP >nul 2>&1'
  if ($LASTEXITCODE -eq 0) { & reg unload HKLM\A04TEMP }
  & cmd.exe /c 'reg query HKLM\SOFTWARE\A04KTEST2 >nul 2>&1'
  if ($LASTEXITCODE -eq 0) { & reg delete HKLM\SOFTWARE\A04KTEST2 /f }
}
