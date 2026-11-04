@echo off
setlocal EnableExtensions
rem Run from the installed Windows with the two tested diagnostics alongside.
reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT >nul 2>&1
if not errorlevel 1 goto winpe
reg query HKLM\SYSTEM\CurrentControlSet\Services\A04Mma8652 >nul 2>&1
if errorlevel 1 goto servicefail
sc query A04Mma8652
if errorlevel 1 goto servicefail
"%~dp0mma8652-status.exe"
if errorlevel 1 goto pnptypefail
"%~dp0mma8652-read.exe"
if errorlevel 1 goto readfail
echo MMA8652_INSTALLED_WINDOWS_RAW_HID_OBSERVED
rem This is not a Windows Sensor API or suspend/resume test.
exit /b 0
:winpe
echo MMA8652_VERIFY_REJECTED_WINPE
exit /b 2
:servicefail
echo MMA8652_INSTALLED_SERVICE_MISSING
exit /b 3
:pnptypefail
echo MMA8652_INSTALLED_PNP_FAILED
exit /b 4
:readfail
echo MMA8652_INSTALLED_HID_READ_FAILED
exit /b 5
