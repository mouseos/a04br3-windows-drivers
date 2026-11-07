# Dedicated normal-Windows MMDevice/WASAPI EXE only. No SYS/tone/media/runner work.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt8167-audio-engine-probe'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'OutputDirectory must be inside this checkout'
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$previousTemp=$env:TEMP;$previousTmp=$env:TMP;$transcript=$false
try {
    $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP
    Start-Transcript -Path (Join-Path $out 'build.log') -Append|Out-Null;$transcript=$true
    $commit=(& git -C $root rev-parse HEAD)
    if($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$'){throw 'Cannot identify probe source commit'}
    $dirty=@(& git -C $root status --porcelain)
    $source=Join-Path $root 'tools\audio\mt8167-audio-engine-probe.c'
    $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1";$inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\um\arm'
    & clang --target=thumbv7-pc-windows-msvc -std=c11 -w -fms-extensions -ffreestanding -fno-builtin -fno-stack-protector -O2 `
      -D_ARM_ -DARM -D_WIN32_WINNT=0x0603 "-DA04_ENGINE_PROBE_SOURCE=0x$($commit.Substring(0,8))u" `
      "-I$inc\um" "-I$inc\shared" -c $source -o (Join-Path $out 'probe.obj')
    if($LASTEXITCODE -ne 0){throw 'Audio engine probe ARM32 compilation failed'}
    $exe=Join-Path $out 'mt8167-audio-engine-probe.exe'
    & lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib `
      (Join-Path $out 'probe.obj') (Join-Path $lib 'kernel32.lib') (Join-Path $lib 'ole32.lib') `
      (Join-Path $lib 'advapi32.lib') (Join-Path $lib 'setupapi.lib') (Join-Path $lib 'cfgmgr32.lib') "/out:$exe"
    if($LASTEXITCODE -ne 0){throw 'Audio engine probe ARM32 link failed'}
    $inspection=@(& llvm-readobj --file-headers --coff-imports $exe)
    if($LASTEXITCODE -ne 0){throw 'Audio engine probe PE inspection failed'}
    $inspection|Set-Content -Encoding UTF8 (Join-Path $out 'pe-imports.txt');$inspection|Write-Host
    Copy-Item (Join-Path $root 'tools\audio\README-mt8167-audio-engine-probe.md') $out
    @{package_version='2026.11.7.4';source_commit=$commit;source_dirty=($dirty.Count -ne 0);
      source_sha256=(Get-FileHash $source -Algorithm SHA256).Hash;
      entry='mt8167-audio-engine-probe.exe (no arguments, no service wrapper)';
      api='shared-mode MMDevice/DeviceTopology/WASAPI; AudioEndpointBuilder and AudioSrv';
      hardware='MediaTek MT8167 AFE / MT6392 Audio';driver_runtime_source_expected='3d83f6fee554455434e2c03112c56bbf351e4c4b';
      audio_inf_source_expected='3d83f6fee554455434e2c03112c56bbf351e4c4b';audio_inf_driverver_expected='11/06/2026,2026.11.7.1007';
      engine_bundle_expected='windows-rt-audio-endpoint-3d83f6f; retained matched parent1010';
      signed_audio_sys_sha256_expected='b917801c0f673aa5ab36be25c4133ad5d1370f1130936bbc4fda7086cf374edc';
      signed_parent_sys_sha256_expected='4638ab23c614aa6537b54b321c2b3bea5e6c54ac02f3c61956a7b1102fac42a5';
      driver_sources_verified_by_probe=$false;diagnostic_interface=11;snapshot_version=1;
      program='two standard client lifetimes; each 48000 frames 1500Hz, 48000 silence, 48000 frames 750Hz; peak 1/256 FS';
      evidence_required='disk log, successful writes and FlushFileBuffers';uart_transport='disabled; owning fixed service reports process start/end; no per-row UART IOCTL';
      output_limit_bytes=16384;output_io='bounded buffered stdout/private file, mandatory FlushFileBuffers';
      snapshot_period_ms=100;snapshot_output='aggregate during capture; baseline, first/last valid each pass, first invalid, terminal snapshot only';
      progress_requires='same-phase running DMA/CUR and IRQ-event changes, no observed faults';
      terminal_snapshot='one read-only observation after API stop, not an independent bus-drain certificate';
      audible_verified=$false;deployment_performed=$false;
      files=@(@{name='mt8167-audio-engine-probe.exe';sha256=(Get-FileHash $exe -Algorithm SHA256).Hash},
              @{name='README-mt8167-audio-engine-probe.md';sha256=(Get-FileHash (Join-Path $out 'README-mt8167-audio-engine-probe.md') -Algorithm SHA256).Hash})
    }|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $out 'probe-manifest.json')
} finally {
    if($transcript){Stop-Transcript|Out-Null}
    $env:TEMP=$previousTemp;$env:TMP=$previousTmp
}
