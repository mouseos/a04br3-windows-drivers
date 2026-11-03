$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\um\arm'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04-disable-quickedit'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\disable-quickedit.c') -o (Join-Path $out 'quickedit.obj')
if ($LASTEXITCODE -ne 0) { throw 'QuickEdit control compilation failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib (Join-Path $out 'quickedit.obj') (Join-Path $lib 'kernel32.lib') "/out:$out\disable-quickedit.exe"
if ($LASTEXITCODE -ne 0) { throw 'QuickEdit control link failed' }
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq 'CN=A04BR3 Touch Development' -and $_.HasPrivateKey } | Sort-Object NotAfter -Descending | Select-Object -First 1
if (-not $cert) { throw 'Touch development signing certificate unavailable' }
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $cert.Thumbprint /s My (Join-Path $out 'disable-quickedit.exe')
if ($LASTEXITCODE -ne 0) { throw 'QuickEdit control signing failed' }
Get-FileHash -Algorithm SHA256 (Join-Path $out 'disable-quickedit.exe')
