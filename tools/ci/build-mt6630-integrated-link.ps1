# Build-only linkage check, never an installable or functional driver.
$ErrorActionPreference='Stop'
& "$PSScriptRoot\build-mt6630-core.ps1"
$priorOidBridge=$env:A04_NDIS_OID_BRIDGE
$priorNvramProbe=$env:A04_NDIS_NVRAM_PROBE
try { $env:A04_NDIS_OID_BRIDGE='1'; $env:A04_NDIS_NVRAM_PROBE='1'; & "$PSScriptRoot\build-mt6630-ndis-candidate.ps1" }
finally { $env:A04_NDIS_OID_BRIDGE=$priorOidBridge; $env:A04_NDIS_NVRAM_PROBE=$priorNvramProbe }
$kit="${env:ProgramFiles(x86)}\Windows Kits\8.1"
$km=Join-Path $kit 'Lib\winv6.3\km\arm'
$core=Join-Path $env:TEMP 'a04br3-mt6630-core'
$ndis=Join-Path $env:TEMP 'a04br3-ndis-candidate'
$sessionSymbols=@()
if ($env:A04_WIFI_SESSION_DIAGNOSTIC -eq '1') { $sessionSymbols=@('/include:a04_windows_session_start','/include:a04_windows_session_stop') }
if ($env:A04_WIFI_SESSION_DIAGNOSTIC -eq '1' -and $env:A04_WIFI_IRQ_DIAGNOSTIC -eq '1') { $sessionSymbols+=@('/include:a04_windows_session_irq_bind','/include:a04_windows_irq_arm') }
$objects=Get-ChildItem "$core\*.obj" | ForEach-Object { $_.FullName }
& lld-link $sessionSymbols /driver /subsystem:native /machine:arm /entry:DriverEntry /nodefaultlib /include:a04_windows_diagnostic_start /include:a04_windows_diagnostic_stop /include:a04_windows_firmware_boot /include:a04_windows_wifi_board_init /include:a04_windows_wifi_control_open /include:a04_wifi_bringup /include:a04_wifi_board_start /include:a04_windows_nvram_init /include:a04_windows_nvram_start /include:a04_windows_nvram_drain /include:a04_wmt_bridge_open /include:a04_wmt_bridge_boot_wmt /include:a04_wmt_bridge_boot_wlan /include:a04_wmt_bridge_tx_mpdu /include:a04_wmt_bridge_tx_ethernet /include:a04_wmt_bridge_rx_ethernet /include:a04_wmt_bridge_tx_event /include:a04_wmt_bridge_scan_begin /include:a04_wmt_bridge_scan_event /include:a04_wmt_bridge_scan_consume /include:a04_wmt_bridge_station_setup /include:a04_wmt_bridge_station_select /include:a04_wmt_bridge_station_event /include:a04_wmt_bridge_station_auth /include:a04_wmt_bridge_station_auth_rx /include:a04_wmt_bridge_station_assoc /include:a04_wmt_bridge_station_assoc_rx /include:a04_wmt_bridge_security_begin /include:a04_wmt_bridge_security_rx /include:a04_wmt_bridge_key_install /include:a04_wmt_bridge_key_event /include:a04_wmt_bridge_authorize /include:a04_native_oid_async_init /include:a04_native_oid_async_submit /include:a04_native_oid_async_cancel /include:a04_native_oid_async_close /include:a04_native_oid_dispatch /include:a04_native_hif_rx_indicate /include:a04_native_hif_rx_decode /include:a04_native_indicate_mpdu /include:a04_native_nbl_send /include:a04_native_nbl_tx_event /include:a04_native_nbl_abort /include:a04_native_scan_fail /include:a04_wmt_bridge_scan_fail /include:a04_native_scan_start /include:a04_native_scan_indicate /include:a04_native_scan_parse /include:a04_wifi_control_start /include:a04_wmt_bridge_poll_wlan_status /include:a04_wmt_bridge_receive /include:a04_wmt_bridge_ack_wlan_status "$ndis\windows_nvram.obj" "$ndis\ndis.obj" "$ndis\native_scan.obj" "$ndis\native_scan_bridge.obj" "$ndis\native_nbl.obj" "$ndis\native_receive.obj" "$ndis\native_hif_rx.obj" "$ndis\native_oid.obj" "$ndis\native_oid_async.obj" $objects "$km\ndis.lib" "$km\ntoskrnl.lib" "$km\hal.lib" "$km\bufferoverflowfastfailk.lib" "/out:$core\mt6630-integrated-link-check.sys"
if($LASTEXITCODE) { throw 'Integrated link check failed' }
Write-Host 'Link check only: Initialize always fails; no INF or device connection.'
