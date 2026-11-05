$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$base=Join-Path $env:RUNNER_TEMP 'a04-toprgu-audit-test'
if ((Test-Path $base) -or (Test-Path 'HKLM:\SOFTWARE\A04TOPRGUTEST') -or (Test-Path 'HKLM:\A04TOPRGUSYS') -or (Test-Path 'HKLM:\A04TOPRGUSW')) { throw 'test namespace already in use' }
New-Item -ItemType Directory -Force "$base\Windows\System32\config","$base\Windows\System32\drivers","$base\ram" | Out-Null
try {
 & reg add HKLM\SOFTWARE\A04TOPRGUTEST\Select /v Current /t REG_DWORD /d 1 /f | Out-Null
 & reg add HKLM\SOFTWARE\A04TOPRGUTEST\ControlSet001\Services\Mt8167Toprgu /v Start /t REG_DWORD /d 0 /f | Out-Null
 & reg add HKLM\SOFTWARE\A04TOPRGUTEST\ControlSet001\Services\Mt8167Toprgu /v Group /t REG_SZ /d 'System Bus Extender' /f | Out-Null
 & reg add HKLM\SOFTWARE\A04TOPRGUTEST\ControlSet001\Services\Mt8167Toprgu /v ImagePath /t REG_EXPAND_SZ /d 'System32\drivers\mt8167-toprgu.sys' /f | Out-Null
 & reg save HKLM\SOFTWARE\A04TOPRGUTEST "$base\Windows\System32\config\SYSTEM" /y | Out-Null
 if($LASTEXITCODE) {throw 'SYSTEM fixture save failed'}
 & reg delete HKLM\SOFTWARE\A04TOPRGUTEST /f | Out-Null
 $thumb='5FC3D0043DE4A471F738FB923FEC87336E81D43C'
 foreach($store in @('Root','TrustedPublisher')) { & reg add "HKLM\SOFTWARE\A04TOPRGUTEST\Microsoft\SystemCertificates\$store\Certificates\$thumb" /v Blob /t REG_BINARY /d 0102 /f | Out-Null }
 & reg save HKLM\SOFTWARE\A04TOPRGUTEST "$base\Windows\System32\config\SOFTWARE" /y | Out-Null
 if($LASTEXITCODE) {throw 'SOFTWARE fixture save failed'}
 & reg delete HKLM\SOFTWARE\A04TOPRGUTEST /f | Out-Null
 [IO.File]::WriteAllText("$base\Windows\System32\drivers\mt8167-toprgu.sys",'fixture')
 [IO.File]::WriteAllText("$base\ram\toprgu-audit-candidate.sys",'fixture')
 [IO.File]::WriteAllText("$base\ram\guard-windows-volume.exe",'guard fixture')
 [IO.File]::WriteAllText("$base\ram\compare-files.exe",'compare fixture')
 [IO.File]::WriteAllText("$base\ram\audit-hive-uncached.exe",'uncached verifier fixture')
 $source=Get-Content (Join-Path $root 'tools\winpe\audit-toprgu-installed.cmd') -Raw
 $source=$source.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\ram")
 $source=$source -replace '(?m)^.*\\guard-windows-volume\.exe\r?$', 'cmd /c exit 0'
 $source=$source -replace '(?m)^.*\\compare-files\.exe ', 'fc /b '
 $source=$source -replace '(?m)^.*\\audit-hive-uncached\.exe .*$', 'cmd /c exit 0'
 [IO.File]::WriteAllText("$base\audit.cmd",$source,[Text.Encoding]::ASCII)
 $systemBefore=(Get-FileHash "$base\Windows\System32\config\SYSTEM").Hash
 $softwareBefore=(Get-FileHash "$base\Windows\System32\config\SOFTWARE").Hash
 $out=& cmd.exe /c "$base\audit.cmd"
 if($LASTEXITCODE -ne 0 -or !($out -match 'TOPRGU_AUDIT_FINISHED_READ_ONLY') -or !($out -match 'TOPRGU_SYS_MATCH=YES') -or !($out -match 'TOPRGU_TEST_CERT_ROOT=YES') -or !($out -match 'TOPRGU_TEST_CERT_PUBLISHER=YES')) {throw ($out -join ' ')}
 if((Get-FileHash "$base\Windows\System32\config\SYSTEM").Hash -ne $systemBefore -or (Get-FileHash "$base\Windows\System32\config\SOFTWARE").Hash -ne $softwareBefore) {throw 'fixture source hives modified'}
 Remove-Item -Recurse -Force "$base\ram\toprgu-audit"
 [IO.File]::WriteAllText("$base\ram\toprgu-audit-candidate.sys",'different')
 $out=& cmd.exe /c "$base\audit.cmd"
 if($LASTEXITCODE -ne 0 -or !($out -match 'TOPRGU_SYS_MATCH=NO')) {throw 'mismatch not reported'}
 Remove-Item -Recurse -Force "$base\ram\toprgu-audit"
 $source=$source -replace '(?m)^cmd /c exit 0\r?$', 'cmd /c exit 1'
 [IO.File]::WriteAllText("$base\reject.cmd",$source,[Text.Encoding]::ASCII)
 $out=& cmd.exe /c "$base\reject.cmd"
 if($LASTEXITCODE -eq 0 -or !($out -match 'TOPRGU_AUDIT_VOLUME_REJECTED') -or (Test-Path "$base\ram\toprgu-audit")) {throw 'guard failure did not block audit'}
 Write-Output 'TOPRGU_INSTALLED_AUDIT_FIXTURE_OK'
} finally {
 foreach($key in @('HKLM\A04TOPRGUSYS','HKLM\A04TOPRGUSW')) { & cmd.exe /c "reg query $key >nul 2>&1"; if($LASTEXITCODE -eq 0) { & reg unload $key | Out-Null } }
 & cmd.exe /c 'reg query HKLM\SOFTWARE\A04TOPRGUTEST >nul 2>&1'; if($LASTEXITCODE -eq 0) { & reg delete HKLM\SOFTWARE\A04TOPRGUTEST /f | Out-Null }
}
$global:LASTEXITCODE=0
