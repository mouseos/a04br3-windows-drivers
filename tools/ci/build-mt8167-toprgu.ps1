$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'mt8167-toprgu'
New-Item -ItemType Directory -Force $out | Out-Null
$dir = Join-Path $root 'drivers\system\mt8167-toprgu'
$screen = if ($env:A04_TOPRGU_UART_SCREEN -eq '1') { '1' } else { '0' }
$handoff = if ($env:A04_TOPRGU_CPU1_HANDOFF -eq '1') { '1' } else { '0' }
$thermal = if ($env:A04_TOPRGU_THERMAL_STOP -eq '1') { '1' } else { '0' }
$objects = @()
$deadline = if ($env:A04_TOPRGU_DEADLINE_SECONDS) { $env:A04_TOPRGU_DEADLINE_SECONDS } else { '0' }
if ($deadline -notmatch '^\d+$' -or [int]$deadline -gt 43200) { throw 'TOPRGU deadline must be 0..43200 seconds' }
foreach ($name in @('wdf_toprgu','reset_command')) {
    $obj = Join-Path $out "$name.obj"
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_TOPRGU_UART_SCREEN=$screen" "-DA04_TOPRGU_CPU1_HANDOFF=$handoff" "-DA04_TOPRGU_DEADLINE_SECONDS=$deadline" "-DA04_TOPRGU_THERMAL_STOP=$thermal" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" "-I$dir" -c (Join-Path $dir "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM32 TOPRGU compilation failed: $name" }
    $objects += $obj
}
$driver = Join-Path $out 'mt8167-toprgu.sys'
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 TOPRGU link failed' }
$headers = & llvm-readobj --file-headers $driver
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected TOPRGU PE format' }
Copy-Item (Join-Path $dir 'mt8167-toprgu.inf') (Join-Path $out 'mt8167-toprgu.inf')
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$out" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'TOPRGU catalog generation failed' }
Write-Host 'TOPRGU prototype built; recovery handoff and sleep behavior are unverified.'
