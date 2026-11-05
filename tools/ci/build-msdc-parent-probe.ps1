# Compile-only KMDF 1.13 parent ABI check; no INF, CAT, signing or deployment.
$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc=Join-Path $kit 'Include'
$lib=Join-Path $kit 'Lib\winv6.3\km\arm'
$wdf=Join-Path $kit 'Lib\wdf\kmdf\arm\1.13'
$temp=if($env:RUNNER_TEMP){$env:RUNNER_TEMP}else{$env:TEMP}
$out=Join-Path $temp 'a04-msdc-parent-probe'
New-Item -ItemType Directory -Force $out|Out-Null
$src=Join-Path $root 'drivers\bus\mt8167-shared'
$filter=if($env:A04_SHARED_EMMC_FILTER_PROBE -eq '1'){'1'}else{'0'}
$children=if($filter -eq '1'){'0'}else{'1'}
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DA04_SHARED_QUERY_EXPERIMENTAL=1 "-DA04_SHARED_CHILD_PROBE=$children" "-DA04_SHARED_EMMC_CHILD_PROBE=$children" "-DA04_SHARED_EMMC_FILTER_PROBE=$filter" -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c (Join-Path $src 'wdf_parent_probe.c') -o (Join-Path $out 'parent.obj')
if($LASTEXITCODE -ne 0){throw 'Parent ARM32 compilation failed'}
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $src 'parent_resources.c') -o (Join-Path $out 'resources.obj')
if($LASTEXITCODE -ne 0){throw 'Resources ARM32 compilation failed'}
foreach ($part in @('pwrap_owner','pmic_read','register_owner','vemc_restore')) {
    $file=if($part -eq 'pmic_read'){Join-Path $root 'drivers\input\a04br3-buttons\pmic_read.c'}else{Join-Path $src "$part.c"}
    & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $file -o (Join-Path $out "$part.obj")
    if($LASTEXITCODE -ne 0){throw "Parent shared core compilation failed: $part"}
}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib (Join-Path $out 'parent.obj') (Join-Path $out 'resources.obj') (Join-Path $out 'pwrap_owner.obj') (Join-Path $out 'pmic_read.obj') (Join-Path $out 'register_owner.obj') (Join-Path $out 'vemc_restore.obj') (Join-Path $wdf 'wdfdriverentry.lib') (Join-Path $wdf 'wdfldr.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$out\parent-probe.sys"
if($LASTEXITCODE -ne 0){throw 'Parent ARM32 link failed'}
& llvm-readobj --file-headers --coff-imports (Join-Path $out 'parent-probe.sys')
if($LASTEXITCODE -ne 0){throw 'Parent PE inspection failed'}
Write-Host 'Non-installable parent ABI probe; D0 always rejects.'
