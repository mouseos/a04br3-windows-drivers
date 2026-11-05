$ErrorActionPreference='Stop'
$base=Join-Path $env:TEMP ('service-restore-test-'+[guid]::NewGuid().ToString('N'))
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$hive="$base\Windows\System32\config\SYSTEM"
if (Test-Path HKLM:\A04TEMP) { throw 'test hive namespace is in use' }
New-Item -ItemType Directory -Force "$base\Windows\System32\config", "$base\Windows\System32\drivers", "$base\Windows\Temp", "$base\src\restore-names" | Out-Null
try {
  & reg add HKLM\SOFTWARE\A04RESTORETEST\Select /v Current /t REG_DWORD /d 1 /f | Out-Null
  & reg add HKLM\SOFTWARE\A04RESTORETEST\ControlSet001\Services\a04msdcboot /v ImagePath /t REG_EXPAND_SZ /d 'System32\drivers\a04msdcprobe.sys' /f | Out-Null
  & reg add HKLM\SOFTWARE\A04RESTORETEST\ControlSet001\Services\A04Touch /v ImagePath /t REG_EXPAND_SZ /d '\SystemRoot\System32\drivers\a04touch.sys' /f | Out-Null
  & reg save HKLM\SOFTWARE\A04RESTORETEST $hive /y | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'hive save failed' }
  & reg delete HKLM\SOFTWARE\A04RESTORETEST /f | Out-Null
  foreach ($f in @('mt8167-msdc.sys','bu21025.sys','a04msdcprobe.sys','a04touch.sys')) {
    [IO.File]::WriteAllText("$base\Windows\System32\drivers\$f",$f)
    [IO.File]::WriteAllText("$base\src\restore-names\$f",$f)
  }
  $batch=Get-Content (Join-Path $root 'tools\winpe\startnet-restore-pre-names-services.cmd') -Raw
  $batch=$batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\src")
  $batch=$batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*$', 'cmd /c exit 0'
  $batch=$batch -replace '(?m)^.*\\(disable-quickedit|guard-windows-volume|check-volume-clean|uart-name-event)\.exe.*$', 'cmd /c exit 0'
  $batch=$batch -replace '(?m)^\s*.*\\compare-files\.exe ', '  fc /b '
  $batch=$batch -replace '(?m)^\s*.*\\copy-backup\.exe ', '  copy /b /y '
  $batch=$batch -replace '(?m)^mountvol C: /p\r?\n',('cmd /c exit 0' + "`r`n")
  [IO.File]::WriteAllText("$base\run.cmd",$batch,[Text.Encoding]::ASCII)
  [IO.File]::WriteAllText("$base\src\restore-names\bu21025.sys",'different')
  $out=& cmd.exe /c "$base\run.cmd"
  if (-not ($out -match 'SERVICE_RESTORE_FAILED')) { throw 'mismatched binary accepted' }
  [IO.File]::WriteAllText("$base\src\restore-names\bu21025.sys",'bu21025.sys')
  $out=& cmd.exe /c "$base\run.cmd"
  if (-not ($out -match 'SERVICE_RESTORE_STAGED')) { throw ($out -join ' ') }
  & reg load HKLM\A04TEMP $hive | Out-Null
  $s=(Get-ItemProperty 'HKLM:\A04TEMP\ControlSet001\Services\a04msdcboot').ImagePath
  $t=(Get-ItemProperty 'HKLM:\A04TEMP\ControlSet001\Services\A04Touch').ImagePath
  & reg unload HKLM\A04TEMP | Out-Null
  if ($s -ne 'System32\drivers\mt8167-msdc.sys' -or $t -ne '\SystemRoot\System32\drivers\bu21025.sys') { throw "unexpected paths: $s $t" }
  Write-Output 'SERVICE_RESTORE_GUARDS_OK'
} finally {
  & cmd.exe /c 'reg query HKLM\A04TEMP >nul 2>&1'
  if ($LASTEXITCODE -eq 0) { & reg unload HKLM\A04TEMP | Out-Null }
  & cmd.exe /c 'reg query HKLM\SOFTWARE\A04RESTORETEST >nul 2>&1'
  if ($LASTEXITCODE -eq 0) { & reg delete HKLM\SOFTWARE\A04RESTORETEST /f | Out-Null }
  Remove-Item -Recurse -Force $base -ErrorAction SilentlyContinue
}
$global:LASTEXITCODE=0
