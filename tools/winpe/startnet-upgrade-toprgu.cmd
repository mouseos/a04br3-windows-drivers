@echo off
title TAB-A04BR3 MT8167 TOPRGU driver update
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\mt8167-toprgu.sys X:\a04br3\toprgu-original.sys
if errorlevel 1 goto reject
if exist C:\Windows\Temp\a04br3-toprgu-upgrade-v3-dism.log goto reject
drvload X:\a04br3\toprgu\mt8167-toprgu.inf
if errorlevel 1 goto providerfail
X:\a04br3\toprgu-provider-status.exe
if errorlevel 1 goto providerfail
echo TOPRGU_UPGRADE_WINPE_STARTED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\toprgu\mt8167-toprgu.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\toprgu-upgrade.log 2>&1
set "A04_RESULT=%errorlevel%"
copy /b X:\a04br3\toprgu-upgrade.log C:\Windows\Temp\a04br3-toprgu-upgrade-v3-dism.log >nul
if errorlevel 1 goto logfail
echo TOPRGU_UPGRADE_DISM_EXIT=%A04_RESULT%
if not "%A04_RESULT%"=="0" goto dismfail
echo TOPRGU_UPGRADE_WINDOWS_STAGED
goto dismount
:reject
echo TOPRGU_UPGRADE_PRECONDITION_REJECTED
goto dismount
:providerfail
echo TOPRGU_UPGRADE_WINPE_FAILED
goto dismount
:logfail
echo TOPRGU_UPGRADE_LOG_SAVE_FAILED
goto dismount
:dismfail
echo TOPRGU_UPGRADE_DISM_FAILED
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
echo TOPRGU_UPGRADE_FINISHED
