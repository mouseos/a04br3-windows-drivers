$ErrorActionPreference='Stop'
if ($env:A04_SCRATCH_LBA -notmatch '^\d+$' -or [uint64]$env:A04_SCRATCH_LBA -gt 4294967280) { throw 'A valid scratch LBA is required' }
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=Join-Path $tempRoot 'msdc-reset-probe'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 ('-DA04_SCRATCH_LBA=' + $env:A04_SCRATCH_LBA) "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\msdc-reset-probe.c') -o (Join-Path $out 'msdc-reset-probe.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 reset probe utility compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'msdc-reset-probe.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'setupapi.lib') "/out:$out\msdc-reset-probe.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 reset probe utility link failed' }
