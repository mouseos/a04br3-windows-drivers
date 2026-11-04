# Build-only PortCls adapter. StartDevice always fails; no install package.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$kit = "${env:ProgramFiles(x86)}\Windows Kits\8.1"
$inc = Join-Path $kit 'Include'
$lib = Join-Path $kit 'Lib\winv6.3\km\arm'
$out = Join-Path $env:TEMP 'a04-mt8167-portcls'
New-Item -ItemType Directory -Force $out | Out-Null
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\adapter.cpp') -o (Join-Path $out 'adapter.obj')
if ($LASTEXITCODE -ne 0) { throw 'PortCls ARM32 compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'adapter.obj') (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') "/out:$out\mt8167-audio-disabled.sys"
if ($LASTEXITCODE -ne 0) { throw 'PortCls ARM32 link failed' }
foreach ($name in @('dl1','dl1_engine','ring','irq','broker_policy','shared_audio')) {
    & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root "drivers\audio\mt8167-afe\$name.c") -o (Join-Path $out "$name.obj")
    if ($LASTEXITCODE -ne 0) { throw "ARM32 helper compilation failed: $name" }
}
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\wavert_dma.cpp') -o (Join-Path $out 'wavert_dma.obj')
if ($LASTEXITCODE -ne 0) { throw 'WaveRT buffer helper compilation failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\common_dma.cpp') -o (Join-Path $out 'common_dma.obj')
if ($LASTEXITCODE -ne 0) { throw 'DMA common buffer helper compilation failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\dma_adapter.cpp') -o (Join-Path $out 'dma_adapter.obj')
if ($LASTEXITCODE -ne 0) { throw 'PnP DMA adapter helper compilation failed' }
foreach ($enabled in @('0','1')) {
    & clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_MT8167_WAVERT_EXPERIMENTAL=$enabled" "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\wavert_registration.cpp') -o (Join-Path $out "wavert_registration_$enabled.obj")
    if ($LASTEXITCODE -ne 0) { throw "WaveRT registration compile failed: $enabled" }
}
foreach ($enabled in @('0','1')) {
    & clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-DA04_MT8167_WAVERT_EXPERIMENTAL=$enabled" "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\irq_windows.cpp') -o (Join-Path $out "irq_windows_$enabled.obj")
    if ($LASTEXITCODE -ne 0) { throw "Windows IRQ helper compilation failed: $enabled" }
}
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\wavert_miniport.cpp') -o (Join-Path $out 'wavert_miniport.obj')
if ($LASTEXITCODE -ne 0) { throw 'WaveRT miniport compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_MT8167_WAVERT_DIAGNOSTIC=1 -DA04_MT8167_OWNER_DIAGNOSTIC=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\adapter.cpp') -o (Join-Path $out 'adapter_diagnostic.obj')
if ($LASTEXITCODE -ne 0) { throw 'WaveRT diagnostic adapter compile failed' }
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $root 'drivers\bus\mt8167-shared\clock_owner.c') -o (Join-Path $out 'clock_owner.obj')
if ($LASTEXITCODE -ne 0) { throw 'Clock owner compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\stream_transport.cpp') -o (Join-Path $out 'stream_transport.obj')
if ($LASTEXITCODE -ne 0) { throw 'Stream transport compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\pnp_backend.cpp') -o (Join-Path $out 'pnp_backend.obj')
if ($LASTEXITCODE -ne 0) { throw 'PnP backend compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\shared_transport.cpp') -o (Join-Path $out 'shared_transport.obj')
if ($LASTEXITCODE -ne 0) { throw 'Shared transport compile failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\audio_parent_client.c') -o (Join-Path $out 'audio_parent_client.obj')
if ($LASTEXITCODE -ne 0) { throw 'Audio parent client compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'adapter_diagnostic.obj') (Join-Path $out 'wavert_miniport.obj') (Join-Path $out 'dl1.obj') (Join-Path $out 'dl1_engine.obj') (Join-Path $out 'wavert_dma.obj') (Join-Path $out 'common_dma.obj') (Join-Path $out 'dma_adapter.obj') (Join-Path $out 'clock_owner.obj') (Join-Path $out 'stream_transport.obj') (Join-Path $out 'pnp_backend.obj') (Join-Path $out 'shared_audio.obj') (Join-Path $out 'shared_transport.obj') (Join-Path $out 'audio_parent_client.obj') (Join-Path $out 'irq_windows_1.obj') (Join-Path $out 'wavert_registration_1.obj') (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$out\mt8167-audio-buildcheck.sys"
if ($LASTEXITCODE -ne 0) { throw 'WaveRT and IRQ integration link failed' }
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\audio_parent_query.c') -o (Join-Path $out 'audio_parent_query.obj')
if ($LASTEXITCODE -ne 0) { throw 'Audio parent query compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_AUDIO_QUERY_SHARED=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\pnp_backend.cpp') -o (Join-Path $out 'pnp_backend_query.obj')
if ($LASTEXITCODE -ne 0) { throw 'Audio parent query backend compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'adapter_diagnostic.obj') (Join-Path $out 'wavert_miniport.obj') (Join-Path $out 'dl1.obj') (Join-Path $out 'dl1_engine.obj') (Join-Path $out 'wavert_dma.obj') (Join-Path $out 'common_dma.obj') (Join-Path $out 'dma_adapter.obj') (Join-Path $out 'clock_owner.obj') (Join-Path $out 'stream_transport.obj') (Join-Path $out 'pnp_backend_query.obj') (Join-Path $out 'audio_parent_query.obj') (Join-Path $out 'shared_audio.obj') (Join-Path $out 'shared_transport.obj') (Join-Path $out 'audio_parent_client.obj') (Join-Path $out 'irq_windows_1.obj') (Join-Path $out 'wavert_registration_1.obj') (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$out\mt8167-audio-query-buildcheck.sys"
if ($LASTEXITCODE -ne 0) { throw 'Audio query integration link failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\afe_mapping.cpp') -o (Join-Path $out 'afe_mapping.obj')
if ($LASTEXITCODE -ne 0) { throw 'AFE mapping compile failed' }
& clang++ --target=thumbv7-pc-windows-msvc -fms-extensions -fms-compatibility -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_AUDIO_QUERY_SHARED=1 -DA04_AUDIO_MAP_AFE=1 "-I$inc\km" "-I$inc\shared" -c (Join-Path $root 'drivers\audio\mt8167-afe\pnp_backend.cpp') -o (Join-Path $out 'pnp_backend_mapping.obj')
if ($LASTEXITCODE -ne 0) { throw 'AFE mapping backend compile failed' }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib (Join-Path $out 'adapter_diagnostic.obj') (Join-Path $out 'wavert_miniport.obj') (Join-Path $out 'dl1.obj') (Join-Path $out 'dl1_engine.obj') (Join-Path $out 'wavert_dma.obj') (Join-Path $out 'common_dma.obj') (Join-Path $out 'dma_adapter.obj') (Join-Path $out 'clock_owner.obj') (Join-Path $out 'stream_transport.obj') (Join-Path $out 'pnp_backend_mapping.obj') (Join-Path $out 'afe_mapping.obj') (Join-Path $out 'audio_parent_query.obj') (Join-Path $out 'shared_audio.obj') (Join-Path $out 'shared_transport.obj') (Join-Path $out 'audio_parent_client.obj') (Join-Path $out 'irq_windows_1.obj') (Join-Path $out 'wavert_registration_1.obj') (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$out\mt8167-audio-mapping-buildcheck.sys"
if ($LASTEXITCODE -ne 0) { throw 'AFE mapping integration link failed' }
Write-Host 'Built default-disabled SYS and diagnostic WaveRT, parent-query and AFE-mapping buildchecks; RUN always fails.'
