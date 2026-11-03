$ErrorActionPreference='Stop'
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot=if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out=Join-Path $tempRoot 'read-emmc-model'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\read-emmc-model.c') -o (Join-Path $out 'read-emmc-model.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 eMMC model utility compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'read-emmc-model.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'advapi32.lib') "/out:$out\read-emmc-model.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 eMMC model utility link failed' }
