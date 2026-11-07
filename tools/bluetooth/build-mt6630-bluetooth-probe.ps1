# WDK8.1 ARM32 EXE only. No driver/parent/native/bulk/service build or deployment.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt6630-bluetooth-probe'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'OutputDirectory must be inside this checkout'}
$commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE){throw 'Cannot identify source commit'}
$dirty=@(& git -C $root status --porcelain);if($LASTEXITCODE -or $dirty.Count){throw 'Build requires a clean fixed source commit'}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$manifest=Join-Path $out 'probe-manifest.json'
if(Test-Path $manifest){Move-Item $manifest (Join-Path $out ('manifest.previous.'+[Guid]::NewGuid().ToString('N')+'.json'))}
$priorTmp=$env:TMP;$priorTemp=$env:TEMP
try {
 $env:TMP=Join-Path $out 'tmp';$env:TEMP=$env:TMP
 $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
 & clang --target=thumbv7-pc-windows-msvc -std=c11 -O2 -fno-stack-protector -ffreestanding -fms-extensions -w -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DNTDDI_VERSION=0x06030000 "-I$inc\um" "-I$inc\shared" -c "$PSScriptRoot\mt6630-bluetooth-probe.c" -o "$out\probe.obj"
 if($LASTEXITCODE){throw 'MT6630 read-only probe compile failed'}
 & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\probe.obj" "$lib\kernel32.lib" "/out:$out\mt6630-bluetooth-probe.exe"
 if($LASTEXITCODE){throw 'MT6630 read-only probe link failed'}
 Copy-Item "$PSScriptRoot\README.md" (Join-Path $out 'README-bluetooth-probe.md')
 @{source_commit=$commit;source_dirty=$false;target='Windows RT 8.1 ARM32 normal OS';status_abi=1;bthx_contract_query_abi=1;
   exe='mt6630-bluetooth-probe.exe';arguments=@();log_name='mt6630-bluetooth-probe.txt';
   kernel_prerequisite='Matching MT6630 Bluetooth driver with fixed cached-status control device';
   standard_api='System32 bthprops.cpl, setupapi.dll, cfgmgr32.dll; dynamically loaded';
   uart='Optional existing A04BR3ScreenUart queue; no MMIO';
   read_only_hardware=$true;hci_command_input=$false;firmware_input=$false;secrets_logged=$false;
   exclusive_bluetooth=$true;wifi_coexistence_verified=$false;signed=$false;hardware_verified=$false;
   sha256=(Get-FileHash (Join-Path $out 'mt6630-bluetooth-probe.exe') -Algorithm SHA256).Hash
 }|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 $manifest
} finally {$env:TMP=$priorTmp;$env:TEMP=$priorTemp}
