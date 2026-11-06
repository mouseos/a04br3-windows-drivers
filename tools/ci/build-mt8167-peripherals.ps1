# Fixed-source WDK8.1 ARM batch: one BEN0001 parent, Audio v11 + KS client,
# USB HID transport and MT6630 Native WLAN. No runner/device/UEFI operations.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build\peripherals-integrated'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(!$out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw 'OutputDirectory must be inside this checkout'
}
$commit=(& git -C $root rev-parse HEAD)
if($LASTEXITCODE -ne 0){throw 'Cannot identify source commit'}
$dirty=@(& git -C $root status --porcelain)
if($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0){throw 'Build requires a clean fixed source commit'}
$audioHeader=[IO.File]::ReadAllText((Join-Path $root 'drivers\bus\mt8167-shared\audio_interface.h'))
if($audioHeader -notmatch '#define A04_SHARED_AUDIO_INTERFACE_VERSION 11\b'){
    throw 'This integration requires the matched Audio v11 parent and child'
}
New-Item -ItemType Directory -Force $out,(Join-Path $out 'tmp')|Out-Null
$overlay=@{
    A04_SHARED_WIFI_RUNTIME='1';A04_MT6392_RTC32K_DIAGNOSTIC='1';A04_SHARED_WIFI_IRQ_DIAGNOSTIC='1';
    A04_SHARED_USB_ONLY='0';A04_SHARED_TOUCH_D0_EXPERIMENT='1';A04_SHARED_BUTTON_D0_EXPERIMENT='1';
    A04_SHARED_APMIXED_RESOURCE='1';A04_USB_GPIO22_SOURCE_EXPERIMENT='1';
    A04_MUSB_HOST_CORE_EXPERIMENT='1';A04_MUSB_HID_CONTROL_EXPERIMENT='1';
    TEMP=(Join-Path $out 'tmp');TMP=(Join-Path $out 'tmp');RUNNER_TEMP=$out
}
$saved=@{};foreach($name in $overlay.Keys){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$audioOut=Join-Path $out 'audio-usb';$wifiOut=Join-Path $out 'wifi';$transcript=$false
$manifestPath=Join-Path $out 'manifest.json'
# Do not leave an older success manifest looking like this build's result.
if(Test-Path $manifestPath){
    $previous='manifest.previous.'+[Guid]::NewGuid().ToString('N')+'.json'
    Move-Item $manifestPath (Join-Path $out $previous)
}
try {
    foreach($name in $overlay.Keys){[Environment]::SetEnvironmentVariable($name,$overlay[$name],'Process')}
    Start-Transcript -Path (Join-Path $out 'build.log') -Append|Out-Null;$transcript=$true
    @{source_commit=$commit;state='building';hardware_verified=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $out 'build-state.json')
    # This leaf builds the shared parent exactly once and both matching children.
    & (Join-Path $PSScriptRoot 'build-mt8167-first-playback.ps1') -IntegratedUsb -OutputDirectory $audioOut
    $audioManifest=Get-Content (Join-Path $audioOut 'manifest.json') -Raw|ConvertFrom-Json
    if($audioManifest.diagnostic_interface -ne 11 -or !$audioManifest.integrated_usb){throw 'Audio/USB build profile mismatch'}
    $required=@('A04_SHARED_WIFI_RUNTIME','A04_MT6392_RTC32K_DIAGNOSTIC','A04_SHARED_WIFI_IRQ_DIAGNOSTIC',
      'A04_SHARED_TOUCH_D0_EXPERIMENT','A04_SHARED_BUTTON_D0_EXPERIMENT','A04_SHARED_FIRST_PLAYBACK',
      'A04_SHARED_DIGITAL_PLAYBACK','A04_SHARED_VOICE_PCM_DIAGNOSTIC','A04_MT6392_CLK_ALIAS_EXPERIMENT',
      'A04_MT6392_UNMUTE_DIAGNOSTIC','A04_USB_GPIO22_SOURCE_EXPERIMENT')
    foreach($name in $required){if($audioManifest.parent_flags.$name -ne '1'){throw "Missing parent feature: $name"}}
    if($audioManifest.parent_flags.A04_SHARED_USB_ONLY -ne '0' -or $audioManifest.parent_flags.A04_SHARED_VOICE_DIAGNOSTIC -ne '0'){
        throw 'Parent must preserve input children and avoid legacy duplicate voice startup'
    }
    $parent=Join-Path $audioOut 'a04-msdc-parent-probe\mt8167-shared.sys'
    $parentHash=(Get-FileHash $parent -Algorithm SHA256).Hash
    # Wi-Fi contributes its miniport/probe, never another differently flagged parent.
    & (Join-Path $PSScriptRoot 'build-mt6630-runtime.ps1') -OutputDirectory $wifiOut -SkipSharedParent
    $wifiManifest=Get-Content (Join-Path $wifiOut 'manifest.json') -Raw|ConvertFrom-Json
    if($wifiManifest.shared_parent_built -ne $false){throw 'Wi-Fi leaf unexpectedly rebuilt the parent'}
    if((Get-FileHash $parent -Algorithm SHA256).Hash -ne $parentHash){throw 'Shared parent changed during Wi-Fi build'}
    $parents=@(Get-ChildItem $out -Recurse -File -Filter 'mt8167-shared.sys')
    if($parents.Count -ne 1){throw 'Batch output must contain exactly one shared parent SYS'}
    & (Join-Path $PSScriptRoot 'build-mt8167-hardware-probe.ps1') -OutputDirectory (Join-Path $out 'hardware-probe') `
      -WlanProbePath (Join-Path $wifiOut 'wlan-probe\mt6630-wlan-probe.exe') `
      -AudioClientPath (Join-Path $audioOut 'client\mt8167-audio-first-playback.exe')
    $files=@(
      'audio-usb\a04-msdc-parent-probe\mt8167-shared.sys',
      'audio-usb\a04-msdc-parent-probe\shared-parent.inf',
      'audio-usb\audio\mt8167-audio.sys','audio-usb\audio\mt8167-afe-first-playback.inf',
      'audio-usb\usb\mt8167-usb20.sys','audio-usb\usb\mt8167-usb20.inf',
      'audio-usb\client\mt8167-audio-first-playback.exe','audio-usb\client\run-mt8167-audio-first-playback.cmd',
      'wifi\mt6630-runtime.sys','wifi\mt6630-runtime.inf','wifi\manifest.json',
      'wifi\wlan-probe\mt6630-wlan-probe.exe','wifi\wlan-probe\mt6630-wlan-observe.cmd',
      'wifi\wlan-probe\probe-manifest.json','wifi\wlan-probe\README-mt6630-wlan-probe.md',
      'hardware-probe\MT8167HardwareProbe.exe','hardware-probe\mt6630-wlan-probe.exe',
      'hardware-probe\mt8167-audio-first-playback.exe','hardware-probe\hardware-probe-manifest.json',
      'hardware-probe\README-mt8167-hardware-probe.md')
    foreach($file in $files){if(!(Test-Path (Join-Path $out $file))){throw "Missing artifact: $file"}}
    foreach($file in $files|Where-Object{$_ -match '\.(sys|exe)$'}){
        $header=& llvm-readobj --file-headers --coff-imports (Join-Path $out $file)
        if($LASTEXITCODE -ne 0 -or ($header -join "`n") -notmatch 'IMAGE_FILE_MACHINE_ARMNT'){
            throw "Unexpected PE machine: $file"
        }
    }
    $client=Get-Content (Join-Path $audioOut 'client\client-manifest.json') -Raw|ConvertFrom-Json
    if($client.diag_interface -ne 11){throw 'KS client/parent diagnostic ABI mismatch'}
    $inputs=Get-Content (Join-Path $root 'platform\a04br3\peripherals-sources.json') -Raw|ConvertFrom-Json
    @{source_commit=$commit;source_dirty=$false;component_sources=$inputs;
      parent_hardware_id='ACPI\BEN0001';shared_parent_count=1;audio_interface=11;usb_interface=1;wifi_interface=2;
      parent_flags=$audioManifest.parent_flags;audio_flags=$audioManifest.audio_flags;
      hardware_probe_service='MT8167HardwareProbe';hardware_probe_path='hardware-probe';hardware_probe_one_attempt=$true;
      namespace='Reuse current BEN0001/touch/battery/buttons/GPU; do not add USB-only SSDT';
      uefi_reference='cc7c20fa (reported by primary owner; not rebuilt or validated here)';
      firmware_embedded=$false;device_calibration_embedded=$false;hardware_verified=$false;
      status='Unsigned WDK8.1 batch completed; signing and target validation are separate';
      prerequisites=@('Matched existing TOPRGU 96-byte UART queue','ARM32 ksuser.dll for KS client',
        'WlanSvc/wlanapi.dll for WLAN probe','Media-prepared MT6630 firmware; runtime device NVRAM',
        'Signed service and two fixed clients staged under System32/MT8167HardwareProbe; trusted machine certificate chain');
      files=@($files|ForEach-Object{@{path=$_;sha256=(Get-FileHash (Join-Path $out $_) -Algorithm SHA256).Hash}})
    }|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $manifestPath
    @{source_commit=$commit;state='complete';hardware_verified=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $out 'build-state.json')
} catch {
    @{source_commit=$commit;state='failed';hardware_verified=$false;error=$_.Exception.Message}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $out 'build-state.json')
    throw
} finally {
    if($transcript){Stop-Transcript|Out-Null}
    foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
}
