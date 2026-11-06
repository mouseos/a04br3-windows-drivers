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
$audioResources=if($env:A04_AUDIO_CHILD_RESOURCES -eq '1'){'1'}else{'0'}
$irqPrep=if($env:A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$afeClock=if($env:A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$topAudio=if($env:A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$apmixed=if($env:A04_SHARED_APMIXED_RESOURCE -eq '1'){'1'}else{'0'}
$trace=if($env:A04_AUDIO_DIAGNOSTIC_TRACE -eq '1'){'1'}else{'0'}
$pmicTrace=if($env:A04_AUDIO_PMIC_TRACE -eq '1'){'1'}else{'0'}
$alias=if($env:A04_MT6392_CLK_ALIAS_EXPERIMENT -eq '1'){'1'}else{'0'}
$thermal=if($env:A04_SHARED_THERMAL_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$voicePcm=if($env:A04_SHARED_VOICE_PCM_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$codecDomain=if($env:A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$rtc32k=if($env:A04_MT6392_RTC32K_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$wifiIrq=if($env:A04_SHARED_WIFI_IRQ_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$voice=if($env:A04_SHARED_VOICE_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$unmute=if($env:A04_MT6392_UNMUTE_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$codec=if($env:A04_SHARED_MT6392_DIAGNOSTIC -eq '1'){'1'}else{'0'}
if($unmute -eq '1' -and $codec -ne '1'){throw 'Unmute requires MT6392 diagnostics'}
$buttonD0=if($env:A04_SHARED_BUTTON_D0_EXPERIMENT -eq '1'){'1'}else{'0'}
$touchD0=if($env:A04_SHARED_TOUCH_D0_EXPERIMENT -eq '1'){'1'}else{'0'}
& clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM -DA04_SHARED_QUERY_EXPERIMENTAL=1 -DA04_SHARED_CHILD_PROBE=1 "-DA04_MT6392_RTC32K_DIAGNOSTIC=$rtc32k" "-DA04_SHARED_WIFI_IRQ_DIAGNOSTIC=$wifiIrq" "-DA04_AUDIO_CHILD_RESOURCES=$audioResources" "-DA04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC=$topAudio" "-DA04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC=$afeClock" "-DA04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC=$irqPrep" "-DA04_SHARED_BUTTON_D0_EXPERIMENT=$buttonD0" "-DA04_SHARED_TOUCH_D0_EXPERIMENT=$touchD0" "-DA04_SHARED_APMIXED_RESOURCE=$apmixed" "-DA04_SHARED_MT6392_DIAGNOSTIC=$codec" "-DA04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC=$codecDomain" "-DA04_SHARED_VOICE_DIAGNOSTIC=$voice" "-DA04_SHARED_VOICE_PCM_DIAGNOSTIC=$voicePcm" "-DA04_SHARED_THERMAL_DIAGNOSTIC=$thermal" "-DA04_MT6392_UNMUTE_DIAGNOSTIC=$unmute" "-DA04_MT6392_WRITE_DIAGNOSTIC=$codec" "-DA04_MT6392_CLK_ALIAS_EXPERIMENT=$alias" "-DA04_AUDIO_DIAGNOSTIC_TRACE=$trace" "-DA04_AUDIO_PMIC_TRACE=$pmicTrace" -DKMDF_VERSION_MAJOR=1 -DKMDF_VERSION_MINOR=13 -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 "-I$inc\km" "-I$inc\shared" "-I$inc\um" "-I$inc\wdf\kmdf\1.13" -c (Join-Path $src 'wdf_parent_probe.c') -o (Join-Path $out 'parent.obj')
if($LASTEXITCODE -ne 0){throw 'Parent ARM32 compilation failed'}
& clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c (Join-Path $src 'parent_resources.c') -o (Join-Path $out 'resources.obj')
if($LASTEXITCODE -ne 0){throw 'Resources ARM32 compilation failed'}
foreach ($part in @('pwrap_owner','pmic_read','register_owner','audio_owner','mt6392_classd','usb_owner','gpio22')) {
    $file=if($part -eq 'pmic_read'){Join-Path $root 'drivers\input\a04br3-buttons\pmic_read.c'}elseif($part -eq 'gpio22'){Join-Path $root 'drivers\usb\mt8167-usb20\gpio22.c'}else{Join-Path $src "$part.c"}
    & clang --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 "-DA04_MT6392_WRITE_DIAGNOSTIC=$codec" "-DA04_MT6392_CLK_ALIAS_EXPERIMENT=$alias" "-DA04_MT6392_UNMUTE_DIAGNOSTIC=$unmute" "-DA04_MT6392_RTC32K_DIAGNOSTIC=$rtc32k" -c $file -o (Join-Path $out "$part.obj")
    if($LASTEXITCODE -ne 0){throw "Parent shared core compilation failed: $part"}
}
& lld-link /driver /subsystem:native /machine:arm /entry:FxDriverEntry /nodefaultlib (Join-Path $out 'parent.obj') (Join-Path $out 'resources.obj') (Join-Path $out 'pwrap_owner.obj') (Join-Path $out 'pmic_read.obj') (Join-Path $out 'register_owner.obj') (Join-Path $out 'audio_owner.obj') (Join-Path $out 'mt6392_classd.obj') (Join-Path $out 'usb_owner.obj') (Join-Path $out 'gpio22.obj') (Join-Path $wdf 'wdfdriverentry.lib') (Join-Path $wdf 'wdfldr.lib') (Join-Path $lib 'ntoskrnl.lib') (Join-Path $lib 'hal.lib') (Join-Path $lib 'bufferoverflowfastfailk.lib') "/out:$out\mt8167-shared.sys"
if($LASTEXITCODE -ne 0){throw 'Parent ARM32 link failed'}
& llvm-readobj --file-headers --coff-imports (Join-Path $out 'mt8167-shared.sys')
if($LASTEXITCODE -ne 0){throw 'Parent PE inspection failed'}
Write-Host "Non-installable parent ABI probe; experimental button-only D0: $buttonD0."
