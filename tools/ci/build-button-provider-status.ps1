$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out = Join-Path $env:RUNNER_TEMP 'a04-button-provider-status'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\button-provider-status.c') -o (Join-Path $out 'status.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 PnP status compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'status.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'cfgmgr32.lib') "/out:$out\button-provider-status.exe"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 PnP status link failed' }
Write-Host "Experimental PnP status executable: $out\button-provider-status.exe"
