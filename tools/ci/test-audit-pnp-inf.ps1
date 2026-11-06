$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base=Join-Path $env:RUNNER_TEMP 'a04-pnp-inf-fixture'
if(Test-Path $base){throw 'fixture exists'}
if(Test-Path 'R:\'){throw 'fixture drive busy'}
New-Item -ItemType Directory -Force "$base\target\Windows\INF","$base\target\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}","$base\target\Windows\System32\DriverStore\FileRepository\fixture.inf_arm_abc" | Out-Null
function Reg([string[]]$a){& reg.exe @a | Out-Null;if($LASTEXITCODE){throw "registry error: $($a -join ' ')"}}
try {
 & subst R: "$base\target";if($LASTEXITCODE){throw 'subst failed'}
 Reg @('add','HKLM\SOFTWARE\A04PNPFIX\Select','/v','Current','/t','REG_DWORD','/d','7','/f')
 Reg @('add','HKLM\SOFTWARE\A04PNPFIX\ControlSet007\Services\Mt8167Toprgu','/v','Start','/t','REG_DWORD','/d','0','/f')
 Reg @('add','HKLM\SOFTWARE\A04PNPFIX\ControlSet007\Enum\ACPI\MOU0005\0&01','/v','Driver','/t','REG_SZ','/d','{ABC}\0001','/f')
 Reg @('add','HKLM\SOFTWARE\A04PNPFIX\ControlSet007\Control\Class\{ABC}\0001','/v','InfPath','/t','REG_SZ','/d','oem42.inf','/f')
 Reg @('save','HKLM\SOFTWARE\A04PNPFIX',"$base\SYSTEM",'/y')
 Reg @('delete','HKLM\SOFTWARE\A04PNPFIX','/f')
 Reg @('load','HKLM\A04AUDITSYS',"$base\SYSTEM")
 [IO.File]::WriteAllText('R:\Windows\INF\oem42.inf',"[Version]`r`nCatalogFile.NTARM=fixture.cat`r`n",[Text.Encoding]::ASCII)
 [IO.File]::WriteAllText('R:\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\fixture.cat','fixture')
 [IO.File]::WriteAllText('R:\Windows\System32\DriverStore\FileRepository\fixture.inf_arm_abc\fixture.cat','store fixture')
 $src=[IO.File]::ReadAllText("$root\tools\winpe\audit-pnp-inf.c").Replace('L"C:\\Windows','L"R:\\Windows')
 [IO.File]::WriteAllText("$base\fixture.c",$src)
 $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\x64'
 & clang --target=x86_64-pc-windows-msvc -fms-extensions -Werror=incompatible-pointer-types -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c "$base\fixture.c" -o "$base\fixture.obj"
 if($LASTEXITCODE){throw 'compile failed'}
 & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\fixture.obj" "$lib\kernel32.lib" "$lib\advapi32.lib" "/out:$base\fixture.exe"
 if($LASTEXITCODE){throw 'link failed'}
 $lines=& "$base\fixture.exe";if($LASTEXITCODE){throw 'audit failed'}
 if(!($lines -contains 'PNP_ENUM_ABSENT=ACPI\BEN0001')){throw 'missing parent enumeration not distinguished'}
 if(!($lines -contains 'BACKUP_SCOPE=DRIVERSTORE STATUS=COMPLETE LOGICAL_BYTES_HEX=000000000000000D FILES_HEX=0000000000000001 DIRS_HEX=0000000000000002')){throw "DriverStore size mismatch: $($lines -join ' | ')"}
 if(!($lines -match '^BACKUP_SCOPE=CATROOT2 STATUS=STAT_ERROR CODE=') -or !($lines -contains 'BACKUP_SCOPE_SIZE_LOGICAL_ONLY_NOT_AUTHORIZATION')){throw 'size failure or scope warning missing'}
 $ih=(Get-FileHash -Algorithm SHA256 'R:\Windows\INF\oem42.inf').Hash;$ch=(Get-FileHash -Algorithm SHA256 'R:\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\fixture.cat').Hash
 if(!($lines -contains 'PNP_CONTROLSET=ControlSet007') -or !($lines -contains 'SERVICE_START=00000000') -or !($lines -contains "PNP_INF=oem42.inf SHA256=$ih") -or !($lines -contains "PNP_CATALOG=fixture.cat SHA256=$ch")){throw "identity/hash mismatch: $($lines -join ' | ')"}
 $sh=(Get-FileHash -Algorithm SHA256 'R:\Windows\System32\DriverStore\FileRepository\fixture.inf_arm_abc\fixture.cat').Hash
 if(!($lines -contains "PNP_CATALOG_STORE_PATH=R:\Windows\System32\DriverStore\FileRepository\fixture.inf_arm_abc\fixture.cat SHA256=$sh") -or !($lines -contains 'PNP_CATALOG_STORE_MATCH_COUNT=00000001 MEMBERSHIP_UNVERIFIED')){throw 'DriverStore catalog discovery failed'}
 foreach($i in 1..8){$d="R:\Windows\System32\DriverStore\FileRepository\extra$i";New-Item -ItemType Directory $d | Out-Null;[IO.File]::WriteAllText("$d\fixture.cat",'extra')}
 $lines=& "$base\fixture.exe"
 if(!($lines -contains 'PNP_CATALOG_STORE_MATCH_LIMIT ENUM_INCOMPLETE') -or @($lines | Where-Object { $_ -like 'PNP_CATALOG_STORE_PATH=*' }).Count -ne 8){throw 'catalog output limit not enforced'}
 foreach($i in 1..8){Remove-Item -Recurse -Force "R:\Windows\System32\DriverStore\FileRepository\extra$i"}
 Remove-Item 'R:\Windows\System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\fixture.cat'
 $lines=& "$base\fixture.exe";if(!($lines -match 'PNP_CATALOG=fixture.cat SHA256=OPEN_ERROR_')){throw 'missing catalog not reported'}
 Reg @('add','HKLM\A04AUDITSYS\ControlSet007\Control\Class\{ABC}\0001','/v','InfPath','/t','REG_SZ','/d','..\escape.inf','/f')
 $lines=& "$base\fixture.exe";if(!($lines -contains 'PNP_INF_REFERENCE_UNRESOLVED')){throw 'unsafe INF reference accepted'}
 Write-Host 'PNP_SELECTED_INF_CATALOG_FIXTURE_PASS (catalog membership not verified)'
} finally {& cmd.exe /c 'reg unload HKLM\A04AUDITSYS >nul 2>&1';& subst R: /d | Out-Null}
