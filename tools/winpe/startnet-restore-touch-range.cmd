@echo off
title TAB-A04BR3 interrupted touch update recovery
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
if exist C:\Windows\System32\drivers\a04touch.sys goto reject
if exist C:\Windows\Temp\a04br3-touch-range-dism.log goto reject
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto serviceexists
reg query HKLM\A04TEMP\ControlSet001\Services\A04Touch >nul 2>&1
if not errorlevel 1 goto serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
drvload X:\a04br3\touch-provider\a04-touch.inf
if errorlevel 1 goto providerfail
X:\a04br3\touch-provider-status.exe
if errorlevel 1 goto providerfail
echo TOUCH_RESTORE_WINPE_STARTED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\touch-provider\a04-touch.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\touch-provider-dism.log 2>&1
set A04_DISM_RESULT=%errorlevel%
copy /b X:\a04br3\touch-provider-dism.log C:\Windows\Temp\a04br3-touch-range-dism.log >nul
if errorlevel 1 goto logfail
type X:\a04br3\touch-provider-dism.log
echo TOUCH_RESTORE_DISM_EXIT=%A04_DISM_RESULT%
if not "%A04_DISM_RESULT%"=="0" goto dismfail
echo TOUCH_RESTORE_WINDOWS_STAGED
goto dismount
:serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
:reject
echo TOUCH_RESTORE_PRECONDITION_REJECTED
goto dismount
:regfail
echo TOUCH_RESTORE_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo TOUCH_RESTORE_HIVE_UNLOAD_FAILED
goto finish
:providerfail
echo TOUCH_RESTORE_WINPE_START_FAILED
goto dismount
:logfail
echo TOUCH_RESTORE_DISM_LOG_SAVE_FAILED
goto dismount
:dismfail
echo TOUCH_RESTORE_DISM_FAILED
goto dismount
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:consolefail
echo QUICKEDIT_DISABLE_FAILED
goto finish
:storagefail
echo STORAGE_DRIVER_FAILED
goto finish
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
:finish
echo TOUCH_RESTORE_STAGING_FINISHED
