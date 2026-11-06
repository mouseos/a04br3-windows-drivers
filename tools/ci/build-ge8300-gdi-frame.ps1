$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
$base=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP};$out=Join-Path $base 'ge8300-gdi-frame'
New-Item -ItemType Directory -Force $out|Out-Null
& clang --target=thumbv7-pc-windows-msvc -w -O2 -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-I$inc\um" "-I$inc\shared" -c (Join-Path $root 'tools\winpe\ge8300-gdi-frame.c') -o "$out\frame.obj"
if($LASTEXITCODE){throw 'GDI workload compile failed'}
& lld-link /subsystem:windows /machine:arm /entry:WinMainCRTStartup /nodefaultlib "$out\frame.obj" "$lib\kernel32.lib" "$lib\user32.lib" "$lib\gdi32.lib" "/out:$out\ge8300-gdi-frame.exe"
if($LASTEXITCODE){throw 'GDI workload link failed'}
