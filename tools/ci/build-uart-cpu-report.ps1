param([Parameter(Mandatory=$true)][string]$Thumbprint)
$ErrorActionPreference='Stop'
if ($Thumbprint -notmatch '^[0-9A-Fa-f]{40}$') {throw 'invalid thumbprint'}
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$out=Join-Path $env:RUNNER_TEMP 'a04-uart-cpu-report';New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\uart-cpu-report.c') -o "$out\report.obj"
if($LASTEXITCODE){throw 'ARM report compilation failed'}
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\report.obj" "$lib\kernel32.lib" "/out:$out\uart-cpu-report.exe"
if($LASTEXITCODE){throw 'ARM report link failed'}
& (Join-Path $kit 'bin\x86\signtool.exe') sign /fd SHA256 /sha1 $Thumbprint /s My "$out\uart-cpu-report.exe"
if($LASTEXITCODE){throw 'ARM report signing failed'}
