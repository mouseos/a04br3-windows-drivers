$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
$out=Join-Path $env:TEMP 'a04br3-mt6630-core'
New-Item -ItemType Directory -Force $out | Out-Null
$irq=if($env:A04_WIFI_IRQ_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$calibration=if($env:A04_WIFI_CALIBRATION_DIAGNOSTIC -eq '1'){'1'}else{'0'}
$session=if($env:A04_WIFI_SESSION_DIAGNOSTIC -eq '1'){'1'}else{'0'}
# Kernel paths do not save FP/SIMD state. Disable both auto-vectorizers,
# VFP/NEON codegen and unaligned wide accesses for every core object too.
foreach($name in @('windows_irq_firmware','windows_session_irq','windows_irq_work','windows_irq','windows_session','windows_diagnostic','windows_calibration','windows_nvram_reader','windows_firmware','sha256','windows_board','resource_lease','bringup','board_sequence','msdc2_pio','power_bridge','power_plan','transport','direct','calibration','nvram_sector','wlan_firmware','windows_msdc2','lifetime','identity','enumerate','control_start','native_tx','regulatory','wmt_bridge')) {
 & clang --target=thumbv7-pc-windows-msvc -w -ffunction-sections -fdata-sections -fms-extensions -ffreestanding -O2 -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM "-DA04_WIFI_CALIBRATION_DIAGNOSTIC=$calibration" "-DA04_WIFI_SESSION_DIAGNOSTIC=$session" "-DA04_WIFI_IRQ_DIAGNOSTIC=$irq" "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\$name.c" -o "$out\$name.obj"
 if($LASTEXITCODE) { throw "$name compile failed" }
 Write-Output "$name ARM32 compile OK"
}
Write-Output 'Compile only: no Windows integration or executable driver.'
