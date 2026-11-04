# Build-only linkage check, never an installable or functional driver.
$ErrorActionPreference='Stop'
& "$PSScriptRoot\build-mt6630-core.ps1"
& "$PSScriptRoot\build-mt6630-ndis-candidate.ps1"
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$core=Join-Path $env:TEMP 'a04br3-mt6630-core'
$ndis=Join-Path $env:TEMP 'a04br3-ndis-candidate'
$objects=Get-ChildItem "$core\*.obj" | ForEach-Object { $_.FullName }
& lld-link /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /include:a04_wmt_bridge_open /include:a04_wmt_bridge_boot_wmt /include:a04_wmt_bridge_boot_wlan /include:a04_wifi_control_start /include:a04_wmt_bridge_poll_wlan_status /include:a04_wmt_bridge_receive /include:a04_wmt_bridge_ack_wlan_status "$ndis\ndis.obj" $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$core\mt6630-integrated-link-check.sys"
if($LASTEXITCODE) { throw 'Integrated link check failed' }
Write-Host 'Link check only: Initialize always fails; no INF or device connection.'
