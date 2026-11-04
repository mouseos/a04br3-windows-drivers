# Build-only ARM32 battery components; this is not an installable driver.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'lc709203f-build-only'
New-Item -ItemType Directory -Force $out | Out-Null
$source = Join-Path $root 'drivers\power\lc709203f'
foreach ($name in @('gauge','mt8167_i2c','battery_class')) {
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $source "$name.c") -o (Join-Path $out "$name.obj")
    if ($LASTEXITCODE -ne 0) { throw "ARM LC709203F compilation failed: $name" }
}
Write-Host 'LC709203F ARM32 build-only components; not device-tested or installable.'
