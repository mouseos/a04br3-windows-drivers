$ErrorActionPreference='Stop'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base=Join-Path $tempRoot 'named-driver-guard-test'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
if ((Test-Path 'HKLM:\A04TEMP') -or (Test-Path 'HKLM:\SOFTWARE\A04NAMEDTEST') -or (Test-Path $base)) { throw 'test namespace already exists' }
New-Item -ItemType Directory -Force "$base\Windows\System32\config","$base\Windows\System32\drivers","$base\Windows\Temp","$base\src" | Out-Null
$hive="$base\Windows\System32\config\SYSTEM"
try {
    & reg add HKLM\SOFTWARE\A04NAMEDTEST\Select /v Current /t REG_DWORD /d 1 /f | Out-Null
    & reg add HKLM\SOFTWARE\A04NAMEDTEST\ControlSet001\Services\a04msdcboot /v ImagePath /t REG_EXPAND_SZ /d 'System32\drivers\a04msdcprobe.sys' /f | Out-Null
    & reg add HKLM\SOFTWARE\A04NAMEDTEST\ControlSet001\Services\Mt8167Toprgu /v Start /t REG_DWORD /d 0 /f | Out-Null
    & reg save HKLM\SOFTWARE\A04NAMEDTEST $hive /y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'test hive save failed' }
    & reg delete HKLM\SOFTWARE\A04NAMEDTEST /f | Out-Null
    [IO.File]::WriteAllText("$base\Windows\System32\drivers\a04msdcprobe.sys",'storage')
    [IO.File]::WriteAllText("$base\src\storage-original.sys",'storage')
    [IO.File]::WriteAllText("$base\Windows\System32\drivers\a04touch.sys",'touch')
    [IO.File]::WriteAllText("$base\src\touch-provider-original.sys",'touch')
    function PrepareBatch([string]$source) {
        $batch=Get-Content (Join-Path $root "tools\winpe\$source") -Raw
        $batch=$batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\src")
        $batch=$batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*$', 'cmd /c exit 0'
        $batch=$batch -replace '(?m)^.*\\(disable-quickedit|guard-windows-volume|read-emmc-model|touch-provider-status|toprgu-provider-status)\.exe.*$', 'cmd /c exit 0'
        $batch=$batch -replace '(?m)^.*\\compare-files\.exe ', 'fc /b '
        $batch=$batch -replace '(?m)^dism.exe .* > (.+?) 2>&1\r?$', 'cmd /c echo MOCK_DISM > $1'
        $batch=$batch -replace '(?m)^mountvol C: /p\r?\n',''
        return $batch
    }
    [IO.File]::WriteAllText("$base\stage.cmd",(PrepareBatch 'startnet-stage-named-devices.cmd'),[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\stage.cmd"
    if (-not ($out -match 'NAMED_DEVICE_WINDOWS_STAGED') -or -not ($out -match 'NAMED_STORAGE_DISM_EXIT=0')) { throw ($out -join ' ') }
    Remove-Item "$base\Windows\Temp\a04br3-named-*-dism.log"
    [IO.File]::WriteAllText("$base\src\storage-original.sys",'different')
    $out=& cmd.exe /c "$base\stage.cmd"
    if (-not ($out -match 'NAMED_DEVICE_PRECONDITION_REJECTED')) { throw 'mismatched storage was accepted' }
    [IO.File]::WriteAllText("$base\src\storage-original.sys",'storage')
    [IO.File]::WriteAllText("$base\Windows\System32\drivers\mt8167-toprgu.sys",'watchdog')
    [IO.File]::WriteAllText("$base\src\toprgu-original.sys",'watchdog')
    [IO.File]::WriteAllText("$base\upgrade.cmd",(PrepareBatch 'startnet-upgrade-toprgu.cmd'),[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\upgrade.cmd"
    if (-not ($out -match 'TOPRGU_UPGRADE_WINDOWS_STAGED') -or -not ($out -match 'TOPRGU_UPGRADE_DISM_EXIT=0')) { throw ($out -join ' ') }
    Remove-Item "$base\Windows\Temp\a04br3-toprgu-upgrade-v7-dism.log"
    [IO.File]::WriteAllText("$base\src\toprgu-original.sys",'different')
    $out=& cmd.exe /c "$base\upgrade.cmd"
    if (-not ($out -match 'TOPRGU_UPGRADE_PRECONDITION_REJECTED')) { throw 'mismatched TOPRGU was accepted' }
    New-Item -ItemType Directory -Force "$base\src\toprgu" | Out-Null
    [IO.File]::WriteAllText("$base\src\toprgu\mt8167-toprgu.sys",'also-different')
    [IO.File]::WriteAllText("$base\disable-toprgu.cmd",(PrepareBatch 'startnet-disable-toprgu.cmd'),[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\disable-toprgu.cmd"
    if (-not ($out -match 'TOPRGU_RECOVERY_PRECONDITION_REJECTED')) { throw 'mismatched TOPRGU recovery was accepted' }
    [IO.File]::WriteAllText("$base\src\toprgu-original.sys",'watchdog')
    $out=& cmd.exe /c "$base\disable-toprgu.cmd"
    if (-not ($out -match 'TOPRGU_RECOVERY_SERVICE_DISABLED')) { throw ($out -join ' ') }
    & reg load HKLM\A04TEMP $hive | Out-Null
    & reg add HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath /t REG_EXPAND_SZ /d 'System32\drivers\mt8167-msdc.sys' /f | Out-Null
    & reg unload HKLM\A04TEMP | Out-Null
    [IO.File]::WriteAllText("$base\Windows\System32\drivers\mt8167-msdc.sys",'current-msdc')
    [IO.File]::WriteAllText("$base\src\msdc-original.sys",'current-msdc')
    [IO.File]::WriteAllText("$base\upgrade-msdc.cmd",(PrepareBatch 'startnet-upgrade-msdc.cmd'),[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\upgrade-msdc.cmd"
    if (-not ($out -match 'MSDC_UPDATE_WINDOWS_STAGED') -or -not ($out -match 'MSDC_UPDATE_DISM_EXIT=0')) { throw ($out -join ' ') }
    Remove-Item "$base\Windows\Temp\a04br3-msdc-update-v5-dism.log"
    [IO.File]::WriteAllText("$base\src\msdc-original.sys",'different')
    $out=& cmd.exe /c "$base\upgrade-msdc.cmd"
    if (-not ($out -match 'MSDC_UPDATE_PRECONDITION_REJECTED')) { throw 'mismatched MSDC was accepted' }
    [IO.File]::WriteAllText("$base\rollback.cmd",(PrepareBatch 'startnet-rollback-named-storage.cmd'),[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\rollback.cmd"
    if (-not ($out -match 'NAMED_STORAGE_ROLLBACK_SERVICE_RESTORED')) { throw ($out -join ' ') }
    & reg load HKLM\A04TEMP $hive | Out-Null
    $value=(Get-ItemProperty 'HKLM:\A04TEMP\ControlSet001\Services\a04msdcboot').ImagePath
    $watchdogStart=(Get-ItemProperty 'HKLM:\A04TEMP\ControlSet001\Services\Mt8167Toprgu').Start
    & reg unload HKLM\A04TEMP | Out-Null
    if ($value -ne 'System32\drivers\a04msdcprobe.sys') { throw "incorrect rollback path: $value" }
    if ($watchdogStart -ne 4) { throw "incorrect watchdog recovery start: $watchdogStart" }
    Write-Output 'NAMED_DRIVER_GUARD_TEST_OK'
} finally {
    & cmd.exe /c 'reg query HKLM\A04TEMP >nul 2>&1'
    if ($LASTEXITCODE -eq 0) { & reg unload HKLM\A04TEMP | Out-Null }
    & cmd.exe /c 'reg query HKLM\SOFTWARE\A04NAMEDTEST >nul 2>&1'
    if ($LASTEXITCODE -eq 0) { & reg delete HKLM\SOFTWARE\A04NAMEDTEST /f | Out-Null }
}
$global:LASTEXITCODE=0
