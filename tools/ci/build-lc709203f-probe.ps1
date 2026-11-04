# Build ARM32 battery test package. It remains disabled and unvalidated.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$wdfInc = Join-Path $inc 'wdf\kmdf\1.13'
$wdfLib = Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$kmLib = Join-Path $kit 'Lib\winv6.3\km\arm'
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'lc709203f-build-only'
New-Item -ItemType Directory -Force $out | Out-Null
$source = Join-Path $root 'drivers\power\lc709203f'
$objects = @()
foreach ($name in @('gauge','mt8167_i2c','battery_class','wdf_gauge')) {
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $source "$name.c") -o (Join-Path $out "$name.obj")
    if ($LASTEXITCODE -ne 0) { throw "ARM LC709203F compilation failed: $name" }
    $objects += Join-Path $out "$name.obj"
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -property installationPath
$vc = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'bin\Hostx64\arm\cl.exe') } | Select-Object -First 1
if (!$vc) { throw 'ARM32 MSVC tools not found' }
$link = Join-Path $vc.FullName 'bin\Hostx64\x64\link.exe'
$package = Join-Path $out 'package'
New-Item -ItemType Directory -Force $package | Out-Null
& $link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib @objects (Join-Path $wdfLib 'wdfdriverentry.lib') (Join-Path $wdfLib 'wdfldr.lib') (Join-Path $kmLib 'ntoskrnl.lib') (Join-Path $kmLib 'hal.lib') (Join-Path $kmLib 'battc.lib') (Join-Path $kmLib 'bufferoverflowfastfailk.lib') (Join-Path $kmLib 'libcntpr.lib') "/out:$package\lc709203f.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM LC709203F link failed' }
Copy-Item (Join-Path $source 'lc709203f.inf') $package -Force
& (Join-Path $kit 'bin\x86\Inf2Cat.exe') "/driver:$package" /os:6_3_ARM /uselocaltime
if ($LASTEXITCODE -ne 0) { throw 'LC709203F catalog generation failed' }
Write-Host 'LC709203F ARM32 test package; not device-validated.'
