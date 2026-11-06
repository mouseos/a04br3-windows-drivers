@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Experimental recovery only. Caller must stop other processes and unload
rem all hives, including any loaded under names unknown to these helpers.
if not exist X:\a04br3\offline-backup.exe goto missing
if not exist X:\a04br3\release-offline-volume.exe goto missing
X:\a04br3\offline-backup.exe --restore
if errorlevel 1 goto restorefail
X:\a04br3\release-offline-volume.exe
if errorlevel 1 goto releasefail
echo OFFLINE_RECOVERY_RELEASED_NTFS_INTEGRITY_UNVERIFIED
exit /b 0
:missing
echo OFFLINE_RECOVERY_HELPER_MISSING
exit /b 2
:restorefail
echo OFFLINE_RECOVERY_FAILED_RETAIN_BACKUP
exit /b 3
:releasefail
echo OFFLINE_RECOVERY_RELEASE_FAILED_RETAIN_BACKUP
exit /b 4
