# Build one matched, retained first-playback parent/PortCls pair. No deployment.
param([string]$OutputDirectory,[switch]$IntegratedUsb,[switch]$IntegratedWifi)
$ErrorActionPreference='Stop'
if($IntegratedWifi -and !$IntegratedUsb){throw 'IntegratedWifi requires IntegratedUsb for the single full-feature parent profile'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\mt8167-first-playback'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'OutputDirectory must be inside this checkout'
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp'),(Join-Path $out 'audio')|Out-Null
$names=@('TEMP','TMP','RUNNER_TEMP','A04_AUDIO_CHILD_RESOURCES','A04_SHARED_QUERY_EXPERIMENTAL',
 'A04_SHARED_CHILD_PROBE','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_APMIXED_RESOURCE',
 'A04_SHARED_MT6392_DIAGNOSTIC','A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC',
 'A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC','A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC',
 'A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC','A04_SHARED_DIGITAL_PLAYBACK','A04_SHARED_FIRST_PLAYBACK',
 'A04_SHARED_VOICE_PCM_DIAGNOSTIC','A04_MT6392_CLK_ALIAS_EXPERIMENT','A04_MT6392_UNMUTE_DIAGNOSTIC',
 'A04_AUDIO_DIAGNOSTIC_TRACE','A04_SHARED_VOICE_DIAGNOSTIC','A04_SHARED_THERMAL_DIAGNOSTIC',
 'A04_SHARED_TOUCH_D0_EXPERIMENT','A04_USB_GPIO22_SOURCE_EXPERIMENT','A04_MT6392_RTC32K_DIAGNOSTIC',
 'A04_SHARED_WIFI_IRQ_DIAGNOSTIC','A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC','A04_SHARED_WIFI_RUNTIME','A04_AUDIO_PMIC_TRACE',
 'A04_SHARED_USB_ONLY','A04_MUSB_HOST_CORE_EXPERIMENT','A04_MUSB_HID_CONTROL_EXPERIMENT')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$transcript=$false
try {
    Start-Transcript -Path (Join-Path $out 'build.log') -Append|Out-Null;$transcript=$true
    foreach($name in $names){if($name.StartsWith('A04_')){[Environment]::SetEnvironmentVariable($name,'0','Process')}}
    $env:TEMP=Join-Path $out 'tmp';$env:TMP=$env:TEMP;$env:RUNNER_TEMP=$out
    foreach($name in @('A04_AUDIO_CHILD_RESOURCES','A04_SHARED_QUERY_EXPERIMENTAL','A04_SHARED_CHILD_PROBE',
      'A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_APMIXED_RESOURCE','A04_SHARED_MT6392_DIAGNOSTIC',
      'A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC','A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC',
      'A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC','A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC',
      'A04_SHARED_DIGITAL_PLAYBACK','A04_SHARED_FIRST_PLAYBACK','A04_SHARED_VOICE_PCM_DIAGNOSTIC',
      'A04_MT6392_CLK_ALIAS_EXPERIMENT','A04_MT6392_UNMUTE_DIAGNOSTIC','A04_AUDIO_DIAGNOSTIC_TRACE')){
        [Environment]::SetEnvironmentVariable($name,'1','Process')
    }
    if($IntegratedUsb){
        $env:A04_SHARED_USB_ONLY='0';$env:A04_SHARED_TOUCH_D0_EXPERIMENT='1'
        $env:A04_USB_GPIO22_SOURCE_EXPERIMENT='1'
        $env:A04_MUSB_HOST_CORE_EXPERIMENT='1';$env:A04_MUSB_HID_CONTROL_EXPERIMENT='1'
        # Existing optional parent consumers keep their resource/feature flags.
        foreach($name in @('A04_SHARED_THERMAL_DIAGNOSTIC','A04_MT6392_RTC32K_DIAGNOSTIC',
          'A04_SHARED_WIFI_IRQ_DIAGNOSTIC','A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC')){
            if($saved[$name] -eq '1'){[Environment]::SetEnvironmentVariable($name,'1','Process')}
        }
    }
    if($IntegratedWifi){
        $env:A04_SHARED_WIFI_RUNTIME='1';$env:A04_MT6392_RTC32K_DIAGNOSTIC='1'
        $env:A04_SHARED_WIFI_IRQ_DIAGNOSTIC='1';$env:A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC='0'
    }
    # Exactly one parent build: includes both the retained audio and USB owners.
    & (Join-Path $PSScriptRoot 'build-msdc-parent-probe.ps1')
    $kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
    $inc=Join-Path $kit 'Include';$lib=Join-Path $kit 'Lib\winv6.3\km\arm'
    $audio=Join-Path $root 'drivers\audio\mt8167-afe';$obj=Join-Path $out 'audio'
    $common=@('--target=thumbv7-pc-windows-msvc','-fms-extensions','-ffreestanding','-fno-stack-protector',
      '-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603',
      "-I$inc\km","-I$inc\shared")
    $flags=@('-DA04_MT8167_WAVERT_EXPERIMENTAL=1','-DA04_MT8167_WAVERT_DIAGNOSTIC=1',
      '-DA04_MT8167_OWNER_DIAGNOSTIC=1','-DA04_AUDIO_QUERY_SHARED=1','-DA04_AUDIO_MAP_AFE=1',
      '-DA04_AUDIO_IRQ_DIAGNOSTIC=1','-DA04_AUDIO_RUN_DIAGNOSTIC=1','-DA04_AUDIO_SILENT_DMA_DIAGNOSTIC=1',
      '-DA04_AUDIO_PCM_PATTERN_DIAGNOSTIC=1','-DA04_AUDIO_UNMUTE_DIAGNOSTIC=1',
      '-DA04_AUDIO_DIGITAL_PLAYBACK=1','-DA04_AUDIO_FIRST_PLAYBACK=1','-DA04_AUDIO_DIAGNOSTIC_TRACE=1')
    $objects=@()
    foreach($name in @('dl1','dl1_engine','shared_audio','audio_parent_client','audio_parent_query')){
        $file=Join-Path $obj "$name.obj";$objects+=$file
        & clang @common @flags -w -c (Join-Path $audio "$name.c") -o $file
        if($LASTEXITCODE -ne 0){throw "Audio C compilation failed: $name"}
    }
    $file=Join-Path $obj 'clock_owner.obj';$objects+=$file
    & clang @common -c (Join-Path $root 'drivers\bus\mt8167-shared\clock_owner.c') -o $file
    if($LASTEXITCODE -ne 0){throw 'Clock owner compilation failed'}
    foreach($name in @('adapter','wavert_miniport','wavert_registration','wavert_dma',
      'common_dma','dma_adapter','stream_transport','pnp_backend','afe_mapping','irq_windows')){
        $file=Join-Path $obj "$name.obj";$objects+=$file
        & clang++ @common @flags -fms-compatibility -fno-exceptions -fno-rtti -c (Join-Path $audio "$name.cpp") -o $file
        if($LASTEXITCODE -ne 0){throw "Audio C++ compilation failed: $name"}
    }
    $binary=Join-Path $obj 'mt8167-audio.sys'
    & lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib @objects `
      (Join-Path $lib 'portcls.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') `
      (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$binary"
    if($LASTEXITCODE -ne 0){throw 'First playback PortCls link failed'}
    & llvm-readobj --file-headers --coff-imports $binary
    if($LASTEXITCODE -ne 0){throw 'Audio PE inspection failed'}
    Copy-Item (Join-Path $audio 'mt8167-afe-first-playback.inf') $obj
    Copy-Item (Join-Path $root 'drivers\bus\mt8167-shared\prototype\shared-parent.inf') (Join-Path $out 'a04-msdc-parent-probe')
    $files=@((Join-Path $out 'a04-msdc-parent-probe\mt8167-shared.sys'),
      (Join-Path $out 'a04-msdc-parent-probe\shared-parent.inf'),$binary,(Join-Path $obj 'mt8167-afe-first-playback.inf'))
    if($IntegratedUsb){
        $usbOut=Join-Path $out 'usb'
        & (Join-Path $PSScriptRoot 'build-mt8167-usb20.ps1') -OutputDirectory $usbOut
        Copy-Item (Join-Path $root 'drivers\usb\mt8167-usb20\mt8167-usb20.inf') $usbOut
        $files+=@((Join-Path $usbOut 'mt8167-usb20.sys'),(Join-Path $usbOut 'mt8167-usb20.inf'))
    }
    if($IntegratedWifi){
        $wifiOut=Join-Path $out 'wifi'
        & (Join-Path $PSScriptRoot 'build-mt6630-runtime.ps1') -OutputDirectory $wifiOut -SharedParentDirectory (Join-Path $out 'a04-msdc-parent-probe')
        $files+=@((Join-Path $wifiOut 'mt6630-runtime.sys'),(Join-Path $wifiOut 'mt6630-runtime.inf'),
          (Join-Path $wifiOut 'manifest.json'),(Join-Path $wifiOut 'wlan-probe\mt6630-wlan-probe.exe'))
    }
    $files+=(Join-Path $out 'a04-msdc-parent-probe\parent-manifest.json')
    $parents=@(Get-ChildItem $out -Recurse -Filter 'mt8167-shared.sys' -File)
    if($parents.Count -ne 1){throw 'Expected exactly one shared parent binary; use a fresh output directory'}
    $hashes=@($files|ForEach-Object{@{path=$_.Substring($out.Length+1);sha256=(Get-FileHash $_ -Algorithm SHA256).Hash}})
    $commit=(& git -C $root rev-parse HEAD);if($LASTEXITCODE -ne 0){throw 'Cannot identify source commit'}
    $dirty=@(& git -C $root status --porcelain)
    $parentFlags=@{};foreach($name in $names){if($name.StartsWith('A04_')){$parentFlags[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}}
    @{source_commit=$commit;source_dirty=($dirty.Count -ne 0);hardware='MediaTek MT8167 AFE / MT6392';
      sample_rate=48000;pcm='private immutable stereo S16_LE, 1500 Hz, peak 128/32768';
      mode='one initial RUN, resources and D0 power reference pinned until reset';
      calibration='MT6392 hardware trim; Linux cold-start DC profile, no measured AU_LOL ADC calibration';
      installable=$false;note='Unsigned pair and INF only; primary owner must sign/package and validate on target';
      integrated_usb=[bool]$IntegratedUsb;integrated_wifi=[bool]$IntegratedWifi;shared_parent_count=$parents.Count;namespace='Reuse existing parent/touch/buttons/battery; no ACPI replacement';
      parent_flags=$parentFlags;audio_flags=$flags;files=$hashes}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
} finally {
    if($transcript){Stop-Transcript|Out-Null}
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
}
