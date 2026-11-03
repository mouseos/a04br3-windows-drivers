$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$temp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $temp 'a04-button-kernel-probe'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c "$root\tools\kernel\button-interface-probe.c" -o "$out\probe.obj"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 kernel probe compilation failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib "$out\probe.obj" (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') "/out:$out\button-interface-probe.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 kernel probe link failed' }
Write-Output "$out\button-interface-probe.sys"
