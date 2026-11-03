$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$output = Join-Path $tempRoot 'a04-touch-x64-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$include = Join-Path $root 'drivers/input/bu21025'
$eintInclude = Join-Path $root 'drivers/bus/mt8167-eint'
$sources = @(
    'drivers/input/bu21025/bu21025.c', 'drivers/input/bu21025/mt8167_adapter.c',
    'drivers/bus/mt8167-i2c/pio.c', 'drivers/input/bu21025/coordinates.c',
    'drivers/input/bu21025/contact.c', 'drivers/input/bu21025/touch_hid.c',
    'drivers/input/bu21025/pipeline.c', 'drivers/input/bu21025/device.c',
    'drivers/bus/mt8167-eint/eint.c'
) | ForEach-Object { Join-Path $root $_ }
foreach ($name in @('bu21025_test', 'bu21025_bus_test', 'touch_coordinates_test',
                    'touch_contact_test', 'touch_hid_test', 'touch_pipeline_test',
                    'touch_device_test', 'eint_test')) {
    $exe = Join-Path $output ($name + '.exe')
    & clang -std=c11 -O2 -Wall -Wextra -Werror "-I$include" "-I$eintInclude" @sources (Join-Path $root "tests/$name.c") -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $name" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
}
