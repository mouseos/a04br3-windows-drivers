$ErrorActionPreference='Stop'
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base=Join-Path $tempRoot 'a04-provider-stage-test'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (Test-Path 'HKLM:\A04TEMP') { throw 'A04TEMP exists' }
if (Test-Path 'HKLM:\SOFTWARE\A04PROVIDERTEST') { throw 'source exists' }
New-Item -ItemType Directory -Force "$base\Windows\System32\config","$base\Windows\System32\drivers","$base\Windows\Temp","$base\src" | Out-Null
$hive="$base\Windows\System32\config\SYSTEM"
try {
    & reg add HKLM\SOFTWARE\A04PROVIDERTEST\Select /v Current /t REG_DWORD /d 1 /f | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'create select failed' }
    & reg add HKLM\SOFTWARE\A04PROVIDERTEST\ControlSet001\Services\A04ButtonProvider /v Start /t REG_DWORD /d 3 /f | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'create service failed' }
    & reg save HKLM\SOFTWARE\A04PROVIDERTEST $hive /y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'save failed' }
    & reg delete HKLM\SOFTWARE\A04PROVIDERTEST /f | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'source removal failed' }
    $batch=Get-Content (Join-Path $root 'tools\winpe\startnet-stage-button-provider.cmd') -Raw
    $batch=$batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\src")
    $batch=$batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*\r?\nif errorlevel 1 goto (storagefail|providerfail)\r?\n','' -replace '(?m)^.*guard-windows-volume.exe\r?\nif errorlevel 1 goto guardfail\r?\n','' -replace '(?m)^.*button-provider-status.exe\r?\nif errorlevel 1 goto providerfail\r?\n','' -replace '(?m)^mountvol C: /p\r?\n',''
    $batch=$batch -replace '(?m)^dism.exe .*$',"cmd /c echo MOCK_DISM > `"$base\src\button-provider-dism.log`""
    [IO.File]::WriteAllText("$base\stage.cmd",$batch,[Text.Encoding]::ASCII)
    $out=& cmd.exe /c "$base\stage.cmd"
    if (-not ($out -match 'BUTTON_PROVIDER_PRECONDITION_REJECTED')) { throw 'Existing service accepted' }
    if (Test-Path "$base\Windows\Temp\a04br3-button-provider-dism.log") { throw 'DISM ran on rejected service' }
    & reg load HKLM\A04TEMP $hive | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'test hive load failed' }
    & reg delete HKLM\A04TEMP\ControlSet001\Services\A04ButtonProvider /f | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'test service deletion failed' }
    & reg unload HKLM\A04TEMP | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'test hive unload failed' }
    $out=& cmd.exe /c "$base\stage.cmd"
    if (-not ($out -match 'BUTTON_PROVIDER_WINDOWS_STAGED')) { throw ($out -join ' ') }
    if (-not (Test-Path "$base\Windows\Temp\a04br3-button-provider-dism.log")) { throw 'DISM log missing' }
    Write-Output 'BUTTON_PROVIDER_STAGE_GUARD_TEST_OK'
} finally {
    & cmd.exe /c 'reg query HKLM\A04TEMP >nul 2>&1'
    if ($LASTEXITCODE -eq 0) { & reg unload HKLM\A04TEMP; if ($LASTEXITCODE -ne 0) { throw 'cleanup unload failed' } }
    & cmd.exe /c 'reg query HKLM\SOFTWARE\A04PROVIDERTEST >nul 2>&1'
    if ($LASTEXITCODE -eq 0) { & reg delete HKLM\SOFTWARE\A04PROVIDERTEST /f; if ($LASTEXITCODE -ne 0) { throw 'cleanup source failed' } }
}
$global:LASTEXITCODE=0
