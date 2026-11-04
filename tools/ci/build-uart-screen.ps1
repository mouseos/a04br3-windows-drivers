$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$out=Join-Path $env:TEMP 'a04br3-uart-screen'
New-Item -ItemType Directory -Force $out | Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\uart-screen.c') -o "$out\screen.obj"
if($LASTEXITCODE) { throw 'screen compile failed' }
& lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib "$out\screen.obj" "$lib\kernel32.lib" "$lib\user32.lib" "$lib\gdi32.lib" "/out:$out\uart-screen.exe"
if($LASTEXITCODE) { throw 'screen link failed' }
