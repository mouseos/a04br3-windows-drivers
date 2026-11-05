$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$base = Join-Path $tempRoot 'mt8167-bu21025-names-stage-test'
if (Test-Path $base) { throw 'Test directory already exists' }
try {
    foreach ($dir in @('Windows\System32\config','Windows\System32\drivers','Windows\INF','Windows\Temp','src\names\storage','src\names\touch')) {
        New-Item -ItemType Directory -Force (Join-Path $base $dir) | Out-Null
    }
    foreach ($pair in @(
        @('Windows\System32\config\SYSTEM','hive-system'), @('Windows\System32\config\SOFTWARE','hive-software'),
        @('Windows\System32\config\SYSTEM.LOG1','log-one'), @('Windows\System32\config\SYSTEM.LOG2','log-two'),
        @('Windows\System32\drivers\a04msdcprobe.sys','storage'), @('src\names\storage\a04msdcprobe.sys','storage'),
        @('Windows\System32\drivers\a04touch.sys','touch'), @('src\names\touch\a04touch.sys','touch'),
        @('Windows\INF\oem32.inf','storage-inf'), @('src\names\original-storage.inf','storage-inf'),
        @('Windows\INF\oem33.inf','touch-inf'), @('src\names\original-touch.inf','touch-inf')
    )) { [IO.File]::WriteAllText((Join-Path $base $pair[0]),$pair[1]) }
    foreach ($name in @('SYSTEM.LOG1','SYSTEM.LOG2')) {
        & attrib +H +S (Join-Path $base "Windows\System32\config\$name")
        if ($LASTEXITCODE) { throw "Cannot set hidden/system attributes: $name" }
    }
    $kit = "${env:ProgramFiles(x86)}\Windows Kits\10"
    $sdk = Get-ChildItem (Join-Path $kit 'Include') -Directory | Sort-Object Name -Descending |
        Where-Object { (Test-Path (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path (Join-Path $kit "Lib\$($_.Name)\um\x64\kernel32.lib")) } |
        Select-Object -First 1
    if (!$sdk) { throw 'Windows x64 SDK missing' }
    $lib = Join-Path $kit "Lib\$($sdk.Name)\um\x64\kernel32.lib"
    & clang --target=x86_64-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_WIN32_WINNT=0x0603 "-I$($sdk.FullName)\um" "-I$($sdk.FullName)\shared" "-I$($sdk.FullName)\ucrt" -c (Join-Path $root 'tools\winpe\copy-backup.c') -o "$base\copy.obj"
    if ($LASTEXITCODE) { throw 'x64 backup helper compilation failed' }
    & lld-link /subsystem:console /machine:x64 /entry:mainCRTStartup /nodefaultlib "$base\copy.obj" $lib "/out:$base\src\copy-backup.exe"
    if ($LASTEXITCODE) { throw 'x64 backup helper link failed' }
    $batch = Get-Content (Join-Path $root 'tools\winpe\startnet-stage-legacy-names.cmd') -Raw
    $batch = $batch.Replace('C:\Windows',"$base\Windows").Replace('X:\a04br3',"$base\src")
    $batch = $batch -replace '(?m)^wpeinit\r?\n','' -replace '(?m)^drvload .*$', 'cmd /c exit 0'
    $batch = $batch -replace '(?m)^.*\\(disable-quickedit|guard-windows-volume|check-volume-clean|uart-name-event)\.exe.*$', 'cmd /c exit 0'
    $batch = $batch -replace '(?m)^\s*.*\\compare-files\.exe ', '    fc /b '
    $batch = $batch -replace '(?m)^\s*dism.exe .* > (.+?) 2>&1\r?$', '  cmd /c echo MOCK_DISM > $1'
    $batch = $batch -replace '(?m)^mountvol C: /p\r?\n',''
    $cmd = Join-Path $base 'stage.cmd'
    [IO.File]::WriteAllText($cmd,$batch,[Text.Encoding]::ASCII)
    $out = & cmd.exe /c $cmd
    if (-not ($out -match 'NAMES_PACKAGES_STAGED_PNP_NOT_VERIFIED')) { throw ($out -join ' ') }
    foreach ($name in @('SYSTEM.LOG1','SYSTEM.LOG2')) {
        if (-not (Test-Path (Join-Path $base "Windows\Temp\mt8167-bu21025-names-stage-v2\$name"))) { throw "Missing registry log backup: $name" }
    }
    & (Join-Path $base 'src\copy-backup.exe') (Join-Path $base 'Windows\System32\config\SYSTEM.LOG1') (Join-Path $base 'Windows\Temp\mt8167-bu21025-names-stage-v2\SYSTEM.LOG1')
    if ($LASTEXITCODE -ne 2) { throw 'Existing backup was not protected' }
    $out = & cmd.exe /c $cmd
    if (-not ($out -match 'NAMES_ALREADY_ATTEMPTED_OR_LOCK_FAILED')) { throw 'Persistent guard failed' }
    Remove-Item -Recurse -Force (Join-Path $base 'Windows\Temp\mt8167-bu21025-names-stage-v2')
    [IO.File]::WriteAllText((Join-Path $base 'src\names\storage\a04msdcprobe.sys'),'different')
    $out = & cmd.exe /c $cmd
    if (-not ($out -match 'NAMES_INSTALLED_PACKAGE_MISMATCH') -or ($out -match 'MOCK_DISM')) { throw 'Mismatch guard failed' }
    Write-Output 'LEGACY_NAME_STAGE_GUARDS_OK'
} finally {
    if (Test-Path $base) { Remove-Item -Recurse -Force $base }
}
