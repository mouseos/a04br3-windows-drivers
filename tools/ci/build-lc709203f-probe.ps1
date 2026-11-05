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
$trace = if ($env:A04_LC709203F_TRACE -eq '1') { '1' } else { '0' }
$charger = if ($env:A04_BQ24160_EXPERIMENTAL -eq '1') { '1' } else { '0' }
$bqhr = if ($env:A04_BQHR_DIAGNOSTIC -eq '1') { '1' } else { '0' }
$clock = if ($env:A04_BQ_CLOCK_DIAGNOSTIC -eq '1') { '1' } else { '0' }
if ($clock -eq '1' -and $bqhr -ne '1') { throw 'Clock observation requires BQHR diagnostic' }
$broker = if ($env:A04_BROKER_CLIENT_EXPERIMENTAL -eq '1') { '1' } else { '0' }
$brokerOwner = if ($env:A04_BROKER_OWNER_EXPERIMENTAL -eq '1') { '1' } else { '0' }
$defaultExit = if ($env:A04_BQ_DEFAULT_EXIT_EXPERIMENTAL -eq '1') { '1' } else { '0' }
if ($defaultExit -eq '1' -and $brokerOwner -ne '1') { throw 'DEFAULT exit requires broker owner' }
if ($broker -eq '1' -and $charger -ne '1') { throw 'Broker client requires BQ experimental components' }
if ($brokerOwner -eq '1' -and ($charger -ne '1' -or $broker -eq '1')) { throw 'Broker owner requires BQ experimental and excludes broker client' }
$objects = @()
foreach ($name in @('gauge','mt8167_i2c','battery_class','wdf_gauge')) {
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_LC709203F_TRACE=$trace" "-DA04_BQ24160_EXPERIMENTAL=$charger" "-DA04_BROKER_CLIENT_EXPERIMENTAL=$broker" "-DA04_BROKER_OWNER_EXPERIMENTAL=$brokerOwner" "-DA04_BQ_DEFAULT_EXIT_EXPERIMENTAL=$defaultExit" "-DA04_BQHR_DIAGNOSTIC=$bqhr" "-DA04_BQ_CLOCK_DIAGNOSTIC=$clock" "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $source "$name.c") -o (Join-Path $out "$name.obj")
    if ($LASTEXITCODE -ne 0) { throw "ARM LC709203F compilation failed: $name" }
    $objects += Join-Path $out "$name.obj"
}
if ($charger -eq '1') {
    $bqSource = Join-Path $root 'drivers\power\bq24160'
    foreach ($name in @('charger','admission','mt8167_i2c','broker_client','broker_wire','broker_wdf','broker_wdf_receiver','broker_wdf_queue','broker_mailbox','owner_broker','broker_handoff','broker_lifecycle','owner_transfer')) {
        $obj = Join-Path $out "bq_$name.obj"
        & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $bqSource "$name.c") -o $obj
        if ($LASTEXITCODE -ne 0) { throw "ARM BQ24160 compilation failed: $name" }
        $objects += $obj
    }
}
if ($clock -eq '1') {
    $bqSource = Join-Path $root 'drivers\power\bq24160'
    $clockSources = @('platform_clock','platform_clock_wdf')
    if ($charger -ne '1') { $clockSources += 'charger' }
    foreach ($name in $clockSources) {
        $obj = Join-Path $out "clock_$name.obj"
        & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $bqSource "$name.c") -o $obj
        if ($LASTEXITCODE -ne 0) { throw "ARM BQ clock observation compilation failed: $name" }
        $objects += $obj
    }
}
if ($bqhr -eq '1' -or $brokerOwner -eq '1') {
    $bqSource = Join-Path $root 'drivers\power\bq24160'
    foreach ($name in @('bqhr_diagnostic','bqhr_acpi')) {
        $obj = Join-Path $out "$name.obj"
        & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$wdfInc" -c (Join-Path $bqSource "$name.c") -o $obj
        if ($LASTEXITCODE -ne 0) { throw "ARM BQHR diagnostic compilation failed: $name" }
        $objects += $obj
    }
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
$umLib = Join-Path $kit 'Lib\winv6.3\um\arm'
foreach ($entry in @(@('lc709203f-status.c','lc709203f-status',''), @('button-provider-status.c','lc709203f-pnp-status','-DA04_BATTERY_PROVIDER_STATUS'))) {
    $src = Join-Path $root "tools\winpe\$($entry[0])"
    $obj = Join-Path $out "$($entry[1]).obj"
    $extra = @(); if ($entry[2]) { $extra += $entry[2] }
    & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 @extra "-I$inc\um" "-I$inc\shared" -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "Battery status compilation failed: $src" }
    & $link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib $obj (Join-Path $umLib 'kernel32.lib') (Join-Path $umLib 'cfgmgr32.lib') "/out:$package\$($entry[1]).exe"
    if ($LASTEXITCODE -ne 0) { throw "Battery status link failed: $src" }
}
Write-Host 'LC709203F ARM32 test package; not device-validated.'
