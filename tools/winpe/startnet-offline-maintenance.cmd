@echo off
rem Experimental maintenance environment: no automatic target writes.
wpeinit
if errorlevel 1 goto initfail
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
if exist E:\EFI\Boot\bootarm.efi goto ready
mountvol E: /s
if errorlevel 1 goto espfail
:ready
if exist E:\A04BACKUP\READY goto journal
if exist E:\A04BACKUP goto incomplete
echo OFFLINE_MAINTENANCE_READY_NO_UPDATE
cmd.exe /k
goto end
:journal
X:\a04br3\offline-backup.exe --probe
if errorlevel 1 goto incomplete
echo OFFLINE_RECOVERY_JOURNAL_AVAILABLE_NO_UPDATE
cmd.exe /k
goto end
:incomplete
echo OFFLINE_RECOVERY_JOURNAL_UNVERIFIED_NO_UPDATE
cmd.exe /k
goto end
:initfail
echo OFFLINE_MAINTENANCE_INIT_FAILED
cmd.exe /k
goto end
:espfail
echo OFFLINE_MAINTENANCE_ESP_MOUNT_FAILED
cmd.exe /k
goto end
:driverfail
echo OFFLINE_MAINTENANCE_STORAGE_FAILED
cmd.exe /k
:end
