$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-pnp-baseline-fixture';if(Test-Path $base){throw 'fixture exists'}
New-Item -ItemType Directory -Force $base | Out-Null
$fixture='HKLM\SOFTWARE\A04PNPUPTEST'
function Reg([string[]]$items){& reg.exe @items | Out-Null;if($LASTEXITCODE){throw "registry failure: $($items -join ' ')"}}
try {
 Reg @('add',"$fixture\Select",'/v','Current','/t','REG_DWORD','/d','1','/f')
 $svc="$fixture\ControlSet001\Services\Mt8167Toprgu"
 Reg @('add',$svc,'/v','Start','/t','REG_DWORD','/d','0','/f')
 Reg @('add',$svc,'/v','Type','/t','REG_DWORD','/d','1','/f')
 Reg @('add',$svc,'/v','ImagePath','/t','REG_SZ','/d','System32\drivers\mt8167-toprgu.sys','/f')
 foreach($x in @(@('MOU0004','{745a17a0-74d3-11d0-b6fe-00a0c90f57da}','0004','oem33.inf'),@('MOU0005','{4d36e97d-e325-11ce-bfc1-08002be10318}','0008','oem35.inf'))){
  Reg @('add',"$fixture\ControlSet001\Enum\ACPI\$($x[0])\0",'/v','Driver','/t','REG_SZ','/d',"$($x[1])\$($x[2])",'/f')
  Reg @('add',"$fixture\ControlSet001\Control\Class\$($x[1])\$($x[2])",'/v','InfPath','/t','REG_SZ','/d',$x[3],'/f')
 }
 $src=[IO.File]::ReadAllText("$root\tools\winpe\offline-pnp-baseline.c").Replace('L"A04AUDITSYS"','L"SOFTWARE\\A04PNPUPTEST"');[IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj";if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe";if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 0 -or !($lines -contains 'PNP_BASELINE_RAM_COPY_MATCH_SNAPSHOT_UNPROVEN')){throw 'valid PnP fixture rejected'}
 Reg @('add',"$fixture\ControlSet001\Control\Class\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}\0004",'/v','InfPath','/t','REG_SZ','/d','oem99.inf','/f')
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'PNP_BASELINE_BINDING_REJECTED')){throw 'mismatched binding accepted'}
 Reg @('add',"$fixture\ControlSet001\Control\Class\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}\0004",'/v','InfPath','/t','REG_SZ','/d','oem33.inf','/f')
 Reg @('add',"$fixture\ControlSet001\Services\Mt8167Shared",'/f')
 $lines=& "$base\fixture.exe";if($LASTEXITCODE -ne 3 -or !($lines -contains 'PNP_BASELINE_SHARED_SERVICE_PRESENT_OR_UNKNOWN')){throw 'unexpected service accepted'}
 Write-Host 'OFFLINE_PNP_BASELINE_SYNTHETIC_PASS (RAM snapshot isolation unproven)'
} finally {& cmd.exe /c "reg delete $fixture /f >nul 2>&1"}
