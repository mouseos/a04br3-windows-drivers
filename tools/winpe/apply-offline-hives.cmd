@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Limited transaction: matching installed SYS files and existing TOPRGU
rem service are required. This does not install packages or establish PnP.
if not exist X:\a04br3\offline-backup.exe goto missing
if not exist X:\a04br3\offline-hive-apply.exe goto missing
if not exist X:\a04br3\release-offline-volume.exe goto missing
if exist E:\A04BACKUP goto existing
X:\a04br3\offline-hive-apply.exe --check
if errorlevel 1 goto rejected
X:\a04br3\offline-backup.exe
if errorlevel 1 goto rejected
X:\a04br3\offline-backup.exe --probe
if errorlevel 1 goto failed
X:\a04br3\offline-backup.exe --arm
if errorlevel 1 goto failed
X:\a04br3\offline-hive-apply.exe --apply
if errorlevel 1 goto rollback
X:\a04br3\release-offline-volume.exe
if errorlevel 1 goto releasefail
X:\a04br3\offline-backup.exe --complete
if errorlevel 1 goto completefail
echo HIVE_TRANSACTION_APPLIED_DRIVER_PNP_UNVERIFIED
exit /b 0
:rollback
X:\a04br3\offline-backup.exe --restore
if errorlevel 1 goto failed
X:\a04br3\release-offline-volume.exe
if errorlevel 1 goto releasefail
X:\a04br3\offline-backup.exe --complete
if errorlevel 1 goto completefail
echo HIVE_TRANSACTION_ROLLED_BACK
exit /b 3
:missing
echo HIVE_TRANSACTION_HELPER_MISSING
exit /b 2
:existing
echo HIVE_TRANSACTION_EXISTING_JOURNAL_RETAINED
exit /b 2
:rejected
echo HIVE_TRANSACTION_REJECTED_NO_HIVE_APPLY
exit /b 2
:failed
echo HIVE_TRANSACTION_RECOVERY_INCOMPLETE_RETAIN_JOURNAL
exit /b 4
:completefail
echo HIVE_TRANSACTION_COMPLETE_MARKER_FAILED_RETAIN_JOURNAL
exit /b 6
:releasefail
echo HIVE_TRANSACTION_RELEASE_FAILED_RETAIN_JOURNAL
exit /b 5
