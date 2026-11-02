$ErrorActionPreference = 'Stop'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$include = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$source = Join-Path $PSScriptRoot '..\..\drivers\storage\mt8167-msdc\miniport.c'
$output = Join-Path $env:RUNNER_TEMP 'a04br3-wdk81-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$object = Join-Path $output 'miniport.obj'
$driver = Join-Path $output 'miniport.sys'
# The old WDK headers produce many diagnostics with current LLVM; compile this
# interface probe separately from production builds and retain the compiler exit code.
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$include\km" "-I$include\shared" "-I$include\um" -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw 'ARM32 interface probe compilation failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib $object (Join-Path $lib 'storport.lib') (Join-Path $lib 'ntoskrnl.lib') "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 interface probe link failed' }
& llvm-readobj --file-headers --coff-imports $driver
if ($LASTEXITCODE -ne 0) { throw 'PE inspection failed' }
