@echo off
title A04BR3 BQHR battery and read-only eMMC
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
mountvol /N
if errorlevel 1 goto automountfail
rem Three-CPU UEFI CPU1 owns watchdog and UART receive. Do not load TOPRGU.
echo COMBINED_BQHR_BATTERY_STORAGE_START
echo BQHR_UART_MARKER_REQUIRED=A04BAT BQHR STOPPED VERIFIED
X:\a04br3\bqhr-test-cert.exe
if errorlevel 1 goto certfail
drvload X:\a04br3\lc709203f\lc709203f.inf
if errorlevel 1 goto batteryloadfail
echo BATTERY_DRVLOAD_OK
X:\a04br3\lc709203f\lc709203f-pnp-status.exe > X:\a04br3\battery-result.txt 2>&1
if errorlevel 1 goto batteryfail
X:\a04br3\lc709203f\lc709203f-status.exe >> X:\a04br3\battery-result.txt 2>&1
if errorlevel 1 goto batteryfail
type X:\a04br3\battery-result.txt
echo BATTERY_QUERY_OK
drvload X:\a04br3\storage-readonly\mt8167-msdc-readonly.inf
if errorlevel 1 goto storagefail
echo MSDC_READONLY_DRVLOAD_OK
X:\a04br3\msdc-read-stress.exe > X:\a04br3\msdc-result.txt 2>&1
if errorlevel 1 goto stressfail
type X:\a04br3\msdc-result.txt
set "MSDC_HASH="
for /f "tokens=1,2 delims==" %%A in (X:\a04br3\msdc-result.txt) do if "%%A"=="MSDC_REFERENCE_SHA256" set "MSDC_HASH=%%B"
if not "%MSDC_HASH%"=="473ec59979015aa168b251f3d31892baf325526adc85e61e04e6538ae74ba0f1" goto referencefail
echo MSDC_REFERENCE_MATCHES_ANDROID_RECOVERY
echo BQHR_REQUIRES_UART_CONFIRMATION
goto finish
:consolefail
echo COMBINED_CONSOLE_FAILED
goto finish
:automountfail
echo COMBINED_AUTOMOUNT_DISABLE_FAILED
goto finish
:certfail
echo BQHR_TEST_CERT_FAILED
goto finish
:batteryloadfail
echo BATTERY_DRVLOAD_FAILED
goto finish
:batteryfail
type X:\a04br3\battery-result.txt
echo BATTERY_QUERY_FAILED
goto finish
:storagefail
echo MSDC_READONLY_DRVLOAD_FAILED
goto finish
:stressfail
type X:\a04br3\msdc-result.txt
echo MSDC_READ_STRESS_FAILED
goto finish
:referencefail
echo MSDC_REFERENCE_MISMATCH_OR_STALE
:finish
echo COMBINED_DIAGNOSTIC_FINISHED
