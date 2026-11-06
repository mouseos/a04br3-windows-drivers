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
$link = Join-Path $vc.FullName 'bin\Hostx64\x64\link.exe'
if (-not (Test-Path $link)) { throw 'MSVC linker not found' }
$ucrtRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$ucrt = Get-ChildItem $ucrtRoot -Directory | Sort-Object Name -Descending | Where-Object { Test-Path (Join-Path $_.FullName 'ucrt\stddef.h') } | Select-Object -First 1
if (-not $ucrt) { throw 'Windows UCRT headers not found' }
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04br3-wddm-display-build-check'
New-Item -ItemType Directory -Force $out | Out-Null
$objects = @()
$sharedDiagnostic = if ($env:A04_GE8300_WDDM_SHARED_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$sharedAutostart = if ($env:A04_GE8300_WDDM_SHARED_AUTOSTART -eq '1') { '1' } else { '0' }
if ($sharedAutostart -eq '1' -and $sharedDiagnostic -ne '1') { throw 'Shared autostart requires shared diagnostic' }
$sharedLease = if ($env:A04_GE8300_WDDM_SHARED_LEASE -eq '1') { '1' } else { '0' }
$direct = if ($env:A04_GE8300_WDDM_DIRECT_EXPERIMENT -eq '1') { '1' } else { '0' }
$fencedTile = if ($env:A04_GE8300_WDDM_FENCED_TILE -eq '1') { '1' } else { '0' }
if ($sharedDiagnostic -eq '1' -and $sharedLease -ne '1') { throw 'Shared diagnostic requires shared lease' }
if ($sharedLease -eq '1' -and $direct -ne '1') { throw 'Shared lease requires direct resource experiment' }
$vendorTile = if ($env:A04_GE8300_WDDM_VENDOR_TILE -eq '1') { '1' } else { '0' }
if ($fencedTile -eq '1' -and $vendorTile -ne '1') { throw 'Fenced tile requires vendor tile' }
$gpuCopy = if ($env:A04_GE8300_WDDM_COPY -eq '1') { '1' } else { '0' }
foreach ($name in @('bdd','bdd_ddi','bdd_dmm','bdd_util','bltfuncs','blthw','memory','ge8300_present','ge8300_trace','ge8300_display_resources')) {
    $obj = Join-Path $out "$name.obj"
    & $cl /nologo /c /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 "/DA04_GE8300_WDDM_SHARED_DIAGNOSTIC=$sharedDiagnostic" "/DA04_GE8300_WDDM_SHARED_AUTOSTART=$sharedAutostart" "/DA04_GE8300_WDDM_SHARED_LEASE=$sharedLease" "/DA04_GE8300_WDDM_DIRECT_EXPERIMENT=$direct" "/DA04_GE8300_WDDM_FENCED_TILE=$fencedTile" "/DA04_GE8300_WDDM_COPY=$gpuCopy" "/DA04_GE8300_WDDM_VENDOR_TILE=$vendorTile" "/I$inc\km" "/I$inc\shared" "/I$inc\um" "/I$($vc.FullName)\include" "/I$($ucrt.FullName)\ucrt" "/I$src" "/Fo$obj" (Join-Path $src "$name.cxx")
    if ($LASTEXITCODE -ne 0) { throw "ARM32 display source compilation failed: $name" }
    $objects += $obj
}
if ($direct -eq '1') {
    foreach ($name in @('vendor_m1_11\windows_shared_scanout','vendor_m1_11\windows_shared_ovl','dma_pages','gpu_mmu')) {
        $obj = Join-Path $out "$($name.Replace('\','-')).obj"
        $gpu = Join-Path $root 'drivers\gpu\mt8167'
        & $cl /nologo /c /TC /std:c11 /kernel /GS- /Zl /D_KERNEL_MODE /D_ARM_ /DARM /DNTDDI_VERSION=0x06030000 /D_WIN32_WINNT=0x0603 "/I$inc\km" "/I$inc\shared" "/I$inc\um" "/I$($vc.FullName)\include" "/I$($ucrt.FullName)\ucrt" "/Fo$obj" (Join-Path $gpu "$name.c")
        if ($LASTEXITCODE -ne 0) { throw "ARM32 shared scanout compilation failed: $name" }
        $objects += $obj
    }
}
& $link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects (Join-Path $lib 'displib.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') (Join-Path $lib 'BufferOverflowGDI.lib') (Join-Path $lib 'libcntpr.lib') "/out:$out\wddm-display-build-check.sys"
if ($LASTEXITCODE -ne 0) { throw 'ARM32 display build-check link failed' }
Write-Host 'Display-only build check; no INF, no GPU acceleration, do not install.'
