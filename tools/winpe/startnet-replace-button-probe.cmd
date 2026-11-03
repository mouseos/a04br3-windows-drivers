@echo off
title TAB-A04BR3 guarded button probe replacement
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
if not exist C:\Windows\System32\a04-button-probe.exe goto mismatch
if exist C:\Windows\System32\a04-button-probe.old goto mismatch
X:\a04br3\compare-files.exe C:\Windows\System32\a04-button-probe.exe X:\a04br3\button-probe-original.exe
if errorlevel 1 goto mismatch
move C:\Windows\System32\a04-button-probe.exe C:\Windows\System32\a04-button-probe.old
if errorlevel 1 goto backupfail
X:\a04br3\compare-files.exe C:\Windows\System32\a04-button-probe.old X:\a04br3\button-probe-original.exe
if errorlevel 1 goto rollback
copy /b /y X:\a04br3\button-probe-service.exe C:\Windows\System32\a04-button-probe.exe
if errorlevel 1 goto rollback
X:\a04br3\compare-files.exe C:\Windows\System32\a04-button-probe.exe X:\a04br3\button-probe-service.exe
if errorlevel 1 goto rollback
echo BUTTON_PROBE_REPLACED
goto dismount
:rollback
del C:\Windows\System32\a04-button-probe.exe >nul 2>&1
move C:\Windows\System32\a04-button-probe.old C:\Windows\System32\a04-button-probe.exe
echo BUTTON_PROBE_REPLACEMENT_FAILED_RESTORE_EXIT=%errorlevel%
goto dismount
:backupfail
echo BUTTON_PROBE_BACKUP_FAILED
goto dismount
:mismatch
echo BUTTON_PROBE_PRECONDITION_FAILED
goto dismount
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
goto finish
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:driverfail
echo STORAGE_DRIVER_FAILED
:finish
echo BUTTON_PROBE_UPDATE_FINISHED
