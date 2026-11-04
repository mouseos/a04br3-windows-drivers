# Compile-only ARM32 component; does not build or install an audio driver.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
if (!(Test-Path (Join-Path $kit 'Include\km\ntddk.h'))) { throw 'WDK 8.1 unavailable' }
$out = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root 'drivers\audio\mt8167-afe\dl1.c') -o (Join-Path $out 'mt8167-dl1.obj')
if ($LASTEXITCODE -ne 0) { throw 'ARM32 DL1 compile failed' }
Write-Host 'ARM32 DL1 component compiled; no driver link or hardware test.'
