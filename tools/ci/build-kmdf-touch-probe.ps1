$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$include = Join-Path $kit 'Include'
$kmdfInclude = Join-Path $include 'wdf\kmdf\1.13'
$kmdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kernelLib = Join-Path $kit 'Lib\winv6.3\km\arm\ntoskrnl.lib'
$securityLib = Join-Path $kit 'Lib\winv6.3\km\arm\bufferoverflowfastfailk.lib'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$output = Join-Path $tempRoot 'a04-kmdf-touch-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$object = Join-Path $output 'probe.obj'
$driver = Join-Path $output 'kmdf-touch-probe.sys'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$include\km" "-I$include\shared" "-I$include\um" "-I$kmdfInclude" -c (Join-Path $root 'tools\ci\kmdf-touch-probe.c') -o $object
if ($LASTEXITCODE -ne 0) { throw 'ARM32 KMDF probe compilation failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib $object (Join-Path $kmdfLib 'wdfdriverentry.lib') (Join-Path $kmdfLib 'wdfldr.lib') $kernelLib $securityLib "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 KMDF probe link failed' }
$headers = & llvm-readobj --file-headers --coff-imports $driver
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected driver PE format' }
Write-Host 'ARM32 KMDF ABI probe built; AddDevice deliberately rejects the device.'
