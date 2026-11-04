$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'mt8167-ge8300-status'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\ge8300-status.c') -o (Join-Path $out 'status.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'status.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\ge8300-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 status link failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DA04_GE8300_PROVIDER_STATUS "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\button-provider-status.c') -o (Join-Path $out 'pnp.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 PnP status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'pnp.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'cfgmgr32.lib') "/out:$out\ge8300-pnp-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 PnP status link failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DA04_GE8300_SERVICE_STATUS "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\ge8300-status.c') -o (Join-Path $out 'service-status.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 service status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'service-status.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\ge8300-service-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 service status link failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DA04_GE8300_COPY_STATUS "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\ge8300-status.c') -o (Join-Path $out 'copy-status.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 copy status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'copy-status.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\ge8300-copy-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM GE8300 copy status link failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 -DA04_DISPLAY_PROVIDER_STATUS "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\button-provider-status.c') -o (Join-Path $out 'display-pnp.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM display PnP status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'display-pnp.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'cfgmgr32.lib') "/out:$out\display-pnp-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM display PnP status link failed' }
