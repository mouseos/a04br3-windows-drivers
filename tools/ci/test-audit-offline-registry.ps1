$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-registry-audit-fixture'
if(Test-Path $base){throw 'fixture exists'}
if(Test-Path 'R:\'){throw 'fixture drive busy'}
New-Item -ItemType Directory -Force "$base\target\a04br3\audit-system" | Out-Null
function Reg([string[]]$a){ & reg.exe @a | Out-Null; if($LASTEXITCODE){throw "reg fixture failed: $($a -join ' ')"} }
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 Reg @('add','HKLM\SOFTWARE\A04FAKESYS\Select','/v','Current','/t','REG_DWORD','/d','1','/f')
 Reg @('add','HKLM\SOFTWARE\A04FAKESYS\ControlSet001\Services\Mt8167Toprgu','/v','Start','/t','REG_DWORD','/d','0','/f')
 Reg @('add','HKLM\SOFTWARE\A04FAKESYS\ControlSet001\Enum\ACPI\MOU0005\0','/v','Driver','/t','REG_SZ','/d','{fixture}\0001','/f')
 Reg @('save','HKLM\SOFTWARE\A04FAKESYS','R:\a04br3\audit-system\SYSTEM','/y')
 Reg @('delete','HKLM\SOFTWARE\A04FAKESYS','/f')
 $batch=[IO.File]::ReadAllText("$root\tools\winpe\audit-offline-registry.cmd").Replace('X:\a04br3','R:\a04br3')
 [IO.File]::WriteAllText("$base\audit.cmd",$batch,[Text.Encoding]::ASCII)
 $result=& cmd.exe /c "`"$base\audit.cmd`" 2>nul";$rc=$LASTEXITCODE
 if($rc -ne 0 -or !($result -match 'A04REG_PRELIMINARY_ONLY_CONTROLSETS_001_004_INF_CAT_UNRESOLVED') -or !($result -match 'MOU0005') -or !($result -match '\{fixture\}\\0001')){throw "fixture audit failed rc=$rc"}
 & cmd.exe /c "reg query HKLM\A04AUDITSYS >nul 2>&1";if(!$LASTEXITCODE){throw 'temporary hive left loaded'}
 Remove-Item 'R:\a04br3\audit-system\SYSTEM'
 $result=& cmd.exe /c "`"$base\audit.cmd`" 2>nul";if($LASTEXITCODE -ne 2 -or !($result -match 'A04REG_RAM_HIVE_MISSING')){throw 'missing hive not rejected'}
 Write-Host 'OFFLINE_REGISTRY_READ_FIXTURE_PASS (PnP selection and INF/CAT unresolved)'
} finally { & cmd.exe /c "reg unload HKLM\A04AUDITSYS >nul 2>&1"; & cmd.exe /c "reg delete HKLM\SOFTWARE\A04FAKESYS /f >nul 2>&1"; & subst R: /d | Out-Null }
