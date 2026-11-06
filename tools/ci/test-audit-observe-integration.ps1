$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
$base=Join-Path $env:RUNNER_TEMP 'audit-observe-integration-fixture'
if((Test-Path $base) -or (Test-Path 'R:\') -or (Test-Path 'S:\')){throw 'fixture exists or drives busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\System32\config\TxR","$base\target\Windows\System32\drivers","$base\target\Windows\INF","$base\target\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}","$base\ram\a04br3" | Out-Null
function Reg([string[]]$a){& reg.exe @a | Out-Null;if($LASTEXITCODE){throw "fixture registry error $($a -join ' ')"}}
function Compile([string]$name,[string]$source,[bool]$fixture){
 [IO.File]::WriteAllText("$base\$name.c",$source)
 $defs=@();if($fixture){$defs+= '-DA04_TEST_FIXTURE'}
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 @defs "-I$inc\um" "-I$inc\shared" -c "$base\$name.c" -o "$base\$name.obj"
 if($LASTEXITCODE){throw "compile $name failed"}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\$name.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:S:\a04br3\$name.exe"
 if($LASTEXITCODE){throw "link $name failed"}
}
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst target failed'}
 & subst S: "$base\ram";if($LASTEXITCODE){throw 'subst RAM failed'}
 Reg @('add','HKLM\SOFTWARE\A04INTEGRATION\Select','/v','Current','/t','REG_DWORD','/d','1','/f')
 Reg @('add','HKLM\SOFTWARE\A04INTEGRATION\ControlSet001\Services\Mt8167Toprgu','/v','Start','/t','REG_DWORD','/d','0','/f')
 Reg @('add','HKLM\SOFTWARE\A04INTEGRATION\ControlSet001\Enum\ACPI\MOU0005\0','/v','Driver','/t','REG_SZ','/d','{ABC}\0001','/f')
 Reg @('add','HKLM\SOFTWARE\A04INTEGRATION\ControlSet001\Control\Class\{ABC}\0001','/v','InfPath','/t','REG_SZ','/d','oem42.inf','/f')
 Reg @('save','HKLM\SOFTWARE\A04INTEGRATION','R:\Windows\System32\config\SYSTEM','/y')
 Reg @('delete','HKLM\SOFTWARE\A04INTEGRATION','/f')
 Reg @('add','HKLM\SOFTWARE\A04INTEGRATION','/f');Reg @('save','HKLM\SOFTWARE\A04INTEGRATION','R:\Windows\System32\config\SOFTWARE','/y');Reg @('delete','HKLM\SOFTWARE\A04INTEGRATION','/f')
 [IO.File]::WriteAllText('R:\Windows\INF\oem42.inf',"[Version]`r`nCatalogFile.NTARM=fixture.cat`r`n",[Text.Encoding]::ASCII)
 [IO.File]::WriteAllText('R:\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\fixture.cat','fixture')
 $before=(Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash
 $copy=[IO.File]::ReadAllText("$root\tools\winpe\audit-copy-hives.c").Replace('L"C:\\Windows','L"R:\\Windows').Replace('L"X:\\a04br3','L"S:\\a04br3')
 $pnp=[IO.File]::ReadAllText("$root\tools\winpe\audit-pnp-inf.c").Replace('L"C:\\Windows','L"R:\\Windows')
 Compile 'audit-copy-hives' $copy $true;Compile 'audit-pnp-inf' $pnp $false
 $stub='#include <windows.h>' + "`n" + 'void mainCRTStartup(void){ExitProcess(0);}'
 foreach($name in @('map-offline-windows','guard-windows-volume','audit-volume-sys','release-offline-volume','uart-audit-stream')){Compile $name $stub $false}
 $batch=[IO.File]::ReadAllText("$root\tools\winpe\run-audit-once.cmd").Replace('X:\a04br3','S:\a04br3')
 [IO.File]::WriteAllText("$base\run.cmd",$batch,[Text.Encoding]::ASCII)
 & cmd.exe /c "$base\run.cmd" | Out-Null;$rc=$LASTEXITCODE
 $report=Get-Content -Raw 'S:\a04br3\toprgu-audit-result.txt'
 if($rc -ne 10 -or $report -notmatch 'HIVE_COPY_VERIFIED_READ_ONLY_NONATOMIC_SNAPSHOT_UNPROVEN' -or $report -notmatch 'PNP_INF=oem42.inf SHA256=' -or $report -notmatch 'SERVICE_START=00000000' -or $report -notmatch 'AUDIT_RELEASE_EXIT=0'){throw "integration failed rc=$rc report=$report"}
 if((Get-FileHash -Algorithm SHA256 'R:\Windows\System32\config\SYSTEM').Hash -ne $before){throw 'original SYSTEM changed'}
 & cmd.exe /c 'reg query HKLM\A04AUDITSYS >nul 2>&1';if(!$LASTEXITCODE){throw 'RAM hive left loaded'}
 Write-Host 'AUDIT_OBSERVE_INTEGRATION_PASS (identity, dirty, release and UART mocked)'
} finally {& cmd.exe /c 'reg unload HKLM\A04AUDITSYS >nul 2>&1';& cmd.exe /c 'reg delete HKLM\SOFTWARE\A04INTEGRATION /f >nul 2>&1';& subst R: /d | Out-Null;& subst S: /d | Out-Null}
