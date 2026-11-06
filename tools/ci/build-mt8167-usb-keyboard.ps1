# Build the two matching ARM32 PnP components. No installation, dispatch,
# SSH, signing, flashing or device control. Run on the main owner's WDK host.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
      [ValidateSet('Integrated','UsbOnly')][string]$Profile='Integrated')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDirectory must be inside this source checkout'
}
if($Profile -eq 'Integrated' -and (Test-Path (Join-Path $out 'UsbKeyboard.asl'))){throw 'Use a separate output directory for integrated mode; standalone ACPI is present'}
New-Item -ItemType Directory -Force $out|Out-Null
$settings=@{
    A04_SHARED_USB_ONLY='1';A04_SHARED_BUTTON_D0_EXPERIMENT='1';A04_SHARED_APMIXED_RESOURCE='1';
    A04_USB_GPIO22_SOURCE_EXPERIMENT='1';A04_MUSB_HOST_CORE_EXPERIMENT='1';A04_MUSB_HID_CONTROL_EXPERIMENT='1';
    A04_SHARED_MT6392_DIAGNOSTIC='0';A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC='0';
    A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC='0';A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC='0';
    A04_SHARED_TOUCH_D0_EXPERIMENT='0';A04_SHARED_THERMAL_DIAGNOSTIC='0';
    A04_SHARED_VOICE_DIAGNOSTIC='0';A04_SHARED_VOICE_PCM_DIAGNOSTIC='0';
    A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC='0';A04_MT6392_RTC32K_DIAGNOSTIC='0';
    A04_SHARED_WIFI_IRQ_DIAGNOSTIC='0';A04_MT6392_UNMUTE_DIAGNOSTIC='0';
    A04_MT6392_CLK_ALIAS_EXPERIMENT='0';A04_AUDIO_CHILD_RESOURCES='0';
    RUNNER_TEMP=(Join-Path $out 'tmp');TEMP=(Join-Path $out 'tmp');TMP=(Join-Path $out 'tmp')
}
# Integrated mode overlays USB admission and keeps the existing feature set;
# it does not turn off audio/thermal/Wi-Fi or replace the platform namespace.
if($Profile -eq 'Integrated') {
    $required=@('A04_SHARED_USB_ONLY','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_APMIXED_RESOURCE',
      'A04_USB_GPIO22_SOURCE_EXPERIMENT','A04_MUSB_HOST_CORE_EXPERIMENT','A04_MUSB_HID_CONTROL_EXPERIMENT',
      'A04_SHARED_TOUCH_D0_EXPERIMENT')
    foreach($key in @($settings.Keys)) {
        if($key.StartsWith('A04_') -and $key -notin $required){$settings.Remove($key)}
    }
    $settings['A04_SHARED_USB_ONLY']='0';$settings['A04_SHARED_TOUCH_D0_EXPERIMENT']='1'
} else {
    $settings['A04_SHARED_FIRST_PLAYBACK']='0';$settings['A04_SHARED_DIGITAL_PLAYBACK']='0';$settings['A04_SHARED_WIFI_RUNTIME']='0'
}
$saved=@{}
foreach($key in $settings.Keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key);[Environment]::SetEnvironmentVariable($key,$settings[$key])}
try {
    New-Item -ItemType Directory -Force $env:RUNNER_TEMP|Out-Null
    & (Join-Path $PSScriptRoot 'build-msdc-parent-probe.ps1')
    & (Join-Path $PSScriptRoot 'build-mt8167-usb20.ps1') -OutputDirectory (Join-Path $out 'usb-objects')
    Copy-Item (Join-Path $env:RUNNER_TEMP 'a04-msdc-parent-probe\mt8167-shared.sys') $out
    Copy-Item (Join-Path $out 'usb-objects\mt8167-usb20.sys') $out
    Copy-Item (Join-Path $root 'drivers\bus\mt8167-shared\prototype\shared-parent.inf') $out
    Copy-Item (Join-Path $root 'drivers\usb\mt8167-usb20\mt8167-usb20.inf') $out
    if($Profile -eq 'UsbOnly'){Copy-Item (Join-Path $root 'platform\a04br3\acpi\UsbKeyboard.asl') $out}
    $manifest=@{
        repository='https://github.com/mouseos/a04br3-windows-drivers.git';commit=(& git -C $root rev-parse HEAD);
        dirty=(@(& git -C $root status --porcelain).Count -ne 0);profile=$Profile;settings=$settings;
        namespace=$(if($Profile -eq 'Integrated'){'Reuse existing shared parent and battery nodes; no ACPI replacement'}else{'Standalone USB-only ACPI candidate'});
        effective_parent_flags=@(Get-ChildItem Env:A04_*|ForEach-Object{@{name=$_.Name;value=$_.Value}});
        files=@(Get-ChildItem $out -File|Where-Object{$_.Name -ne 'manifest.json'}|ForEach-Object{
            @{name=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}
        });status='Unsigned WDK build only; not hardware verified'
    }
    $manifest|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $out 'manifest.json')
} finally {
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}
}
Write-Host "$Profile shared parent and HID transport built. Keep existing input/battery packages; hardware remains unverified."
