$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$inc="${env:ProgramFiles(x86)}\Windows Kits\8.1\Include"
$out=Join-Path $env:TEMP 'a04br3-mt6630-core'
New-Item -ItemType Directory -Force $out | Out-Null
foreach($name in @('windows_diagnostic','windows_firmware','sha256','windows_board','resource_lease','bringup','board_sequence','msdc2_pio','power_bridge','power_plan','transport','direct','calibration','nvram_sector','wlan_firmware','windows_msdc2','lifetime','identity','enumerate','control_start','native_tx','wmt_bridge')) {
 & clang --target=thumbv7-pc-windows-msvc -w -fms-extensions -ffreestanding -fno-stack-protector -D_KERNEL_MODE -D_ARM_ -DARM "-I$inc\km" "-I$inc\shared" "-I$inc\um" -c "$root\drivers\network\mt6630\$name.c" -o "$out\$name.obj"
 if($LASTEXITCODE) { throw "$name compile failed" }
 Write-Output "$name ARM32 compile OK"
}
Write-Output 'Compile only: no Windows integration or executable driver.'
