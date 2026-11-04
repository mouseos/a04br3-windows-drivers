# Compile the Microsoft-derived display-only code with the Windows 8.1 ABI.
# This build check is not an installable device driver or GPU acceleration.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$src = Join-Path $root 'drivers\gpu\mt8167\wddm-display'
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -property installationPath
if (-not $vs) { throw 'Visual Studio with ARM32 MSVC is required' }
$tools = Join-Path $vs 'VC\Tools\MSVC'
$vc = Get-ChildItem $tools -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'bin\Hostx64\arm\cl.exe') } | Select-Object -First 1
if (-not $vc) { throw 'ARM32 MSVC compiler not found' }
$cl = Join-Path $vc.FullName 'bin\Hostx64\arm\cl.exe'
$ucrtRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$ucrt = Get-ChildItem $ucrtRoot -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'ucrt\stddef.h') } | Select-Object -First 1
if (-not $ucrt) { throw 'Windows UCRT headers not found' }
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04br3-wddm-display-build-check'
New-Item -ItemType Directory -Force $out | Out-Null
$objects = @()
foreach ($name in @('bdd','bdd_ddi','bdd_dmm','bdd_util','bltfuncs','blthw','memory')) {
    $obj = Join-Path $out "$name.obj"
    & $cl /nologo /c /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 "/I$inc\km" "/I$inc\shared" "/I$inc\um" "/I$($vc.FullName)\include" "/I$($ucrt.FullName)\ucrt" "/I$src" "/Fo$obj" (Join-Path $src "$name.cxx")
    if ($LASTEXITCODE -ne 0) { throw "ARM32 display source compilation failed: $name" }
    $objects += $obj
}
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects (Join-Path $lib 'displib.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') (Join-Path $lib 'BufferOverflowGDI.lib') (Join-Path $lib 'libcntpr.lib') "/out:$out\wddm-display-build-check.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 display build-check link failed' }
Write-Host 'Display-only build check; no INF, no GPU acceleration, do not install.'
