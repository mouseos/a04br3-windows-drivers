@echo off
title TAB-A04BR3 experimental button provider removal
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
set "A04_MATCH_COUNT=0"
set "A04_DRIVER_ID="
for %%F in (C:\Windows\inf\oem*.inf) do if exist "%%~fF" call :match "%%~fF" "%%~nxF"
if not "%A04_MATCH_COUNT%"=="1" goto reject
echo BUTTON_PROVIDER_REMOVE_DRIVER=%A04_DRIVER_ID%
dism.exe /Image:C:\ /Remove-Driver "/Driver:%A04_DRIVER_ID%" /ScratchDir:X:\Windows\Temp > X:\a04br3\button-provider-remove.log 2>&1
set A04_DISM_RESULT=%errorlevel%
type X:\a04br3\button-provider-remove.log
echo BUTTON_PROVIDER_REMOVE_EXIT=%A04_DISM_RESULT%
if not "%A04_DISM_RESULT%"=="0" goto dismfail
echo BUTTON_PROVIDER_WINDOWS_REMOVED
goto dismount
:match
X:\a04br3\compare-files.exe "%~1" X:\a04br3\button-provider\a04-button-provider.inf >nul 2>&1
if errorlevel 1 exit /b 0
set /a A04_MATCH_COUNT+=1
set "A04_DRIVER_ID=%~2"
exit /b 0
:reject
echo BUTTON_PROVIDER_REMOVE_MATCH_REJECTED
goto dismount
:dismfail
echo BUTTON_PROVIDER_REMOVE_FAILED
goto dismount
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:storagefail
echo STORAGE_DRIVER_FAILED
goto finish
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
:finish
echo BUTTON_PROVIDER_REMOVAL_FINISHED
