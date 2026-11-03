@echo off
title TAB-A04BR3 guarded touch range update
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04touch.sys X:\a04br3\touch-provider-original.sys
if errorlevel 1 goto reject
if exist C:\Windows\Temp\a04br3-touch-range-dism.log goto reject
set "A04_MATCH_COUNT=0"
set "A04_DRIVER_ID="
for %%F in (C:\Windows\inf\oem*.inf) do if exist "%%~fF" call :match "%%~fF" "%%~nxF"
if not "%A04_MATCH_COUNT%"=="1" goto reject
drvload X:\a04br3\touch-provider\bu21025.inf
if errorlevel 1 goto providerfail
X:\a04br3\touch-provider-status.exe
if errorlevel 1 goto providerfail
echo TOUCH_RANGE_WINPE_STARTED
dism.exe /Image:C:\ /Remove-Driver "/Driver:%A04_DRIVER_ID%" /ScratchDir:X:\Windows\Temp > X:\a04br3\touch-remove.log 2>&1
if errorlevel 1 goto removefail
echo TOUCH_RANGE_OLD_REMOVED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\touch-provider\bu21025.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\touch-add.log 2>&1
set A04_DISM_RESULT=%errorlevel%
copy /b X:\a04br3\touch-add.log C:\Windows\Temp\a04br3-touch-range-dism.log >nul
if errorlevel 1 goto logfail
type X:\a04br3\touch-add.log
echo TOUCH_RANGE_DISM_EXIT=%A04_DISM_RESULT%
if not "%A04_DISM_RESULT%"=="0" goto dismfail
echo TOUCH_RANGE_WINDOWS_STAGED
goto dismount
:match
X:\a04br3\compare-files.exe "%~1" X:\a04br3\touch-provider-original.inf >nul 2>&1
if errorlevel 1 exit /b 0
set /a A04_MATCH_COUNT+=1
set "A04_DRIVER_ID=%~2"
exit /b 0
:reject
echo TOUCH_RANGE_PRECONDITION_REJECTED
goto dismount
:providerfail
echo TOUCH_RANGE_WINPE_START_FAILED
goto dismount
:removefail
echo TOUCH_RANGE_REMOVE_FAILED
type X:\a04br3\touch-remove.log
goto dismount
:logfail
echo TOUCH_RANGE_LOG_SAVE_FAILED
goto dismount
:dismfail
echo TOUCH_RANGE_DISM_FAILED
goto dismount
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:storagefail
echo STORAGE_DRIVER_FAILED
goto finish
:consolefail
echo QUICKEDIT_DISABLE_FAILED
goto finish
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
:finish
echo TOUCH_RANGE_UPDATE_FINISHED
