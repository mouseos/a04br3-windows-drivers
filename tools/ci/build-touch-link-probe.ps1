$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$include = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm\ntoskrnl.lib'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$output = Join-Path $tempRoot 'a04-touch-link-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$probe = Join-Path $output 'probe.obj'
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$include\km" "-I$include\shared" "-I$include\um" -c (Join-Path $root 'tools\ci\touch-link-probe.c') -o $probe
if ($LASTEXITCODE -ne 0) { throw 'ARM32 WDK probe compilation failed' }
$objects = @($probe)
foreach ($item in @(
    @('bu21025','drivers\input\bu21025\bu21025.c'),
    @('boardinit','drivers\input\bu21025\board_init.c'),
    @('adapter','drivers\input\bu21025\mt8167_adapter.c'),
    @('coordinates','drivers\input\bu21025\coordinates.c'),
    @('touchhid','drivers\input\bu21025\touch_hid.c'),
    @('contact','drivers\input\bu21025\contact.c'),
    @('pipeline','drivers\input\bu21025\pipeline.c'),
    @('touchdevice','drivers\input\bu21025\device.c'),
    @('i2c','drivers\bus\mt8167-i2c\pio.c'),
    @('eint','drivers\bus\mt8167-eint\eint.c')
)) {
    $obj = Join-Path $output ($item[0] + '.obj')
    & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root $item[1]) -o $obj
    if ($LASTEXITCODE -ne 0) { throw "ARM32 compile failed: $($item[0])" }
    $objects += $obj
}
$driver = Join-Path $output 'touch-link-probe.sys'
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects $lib "/out:$driver"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 WDK probe link failed' }
$headers = & llvm-readobj --file-headers --coff-imports $driver
if ($LASTEXITCODE -ne 0 -or ($headers -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT') { throw 'Unexpected driver PE format' }
Write-Host 'ARM32 touch link probe built; it is non-operational and intentionally refuses to load.'
