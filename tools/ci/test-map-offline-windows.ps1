$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$out = Join-Path $tempRoot 'a04-map-offline-windows-host-test.exe'
& clang -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-sometimes-uninitialized -D_CRT_SECURE_NO_WARNINGS "-I$root\tests\offline-release-shim" "$root\tests\map_offline_windows_test.c" -o $out -Xlinker /defaultlib:libucrt -Xlinker /defaultlib:libvcruntime
if ($LASTEXITCODE) { throw 'mapper synthetic build failed' }
& $out
if ($LASTEXITCODE) { throw 'mapper synthetic execution failed' }
