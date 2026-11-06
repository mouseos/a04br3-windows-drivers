@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Run only after the transaction has closed every target file and unloaded
rem every hive it owns. A guard failure or unknown hive state forbids dismount.
if not exist X:\a04br3\guard-windows-volume.exe goto reject
if not exist X:\a04br3\flush-toprgu-volume.exe goto reject
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto reject
X:\a04br3\flush-toprgu-volume.exe
if errorlevel 1 goto flushfail
mountvol C: /p
set "A04_MOUNTVOL_RC=%errorlevel%"
echo TOPRGU_MOUNTVOL_EXIT=%A04_MOUNTVOL_RC%
if not "%A04_MOUNTVOL_RC%"=="0" goto dismountfail
echo TOPRGU_TRANSACTION_VOLUME_RELEASED
exit /b 0
:reject
echo TOPRGU_TRANSACTION_RELEASE_GUARD_REJECTED
exit /b 2
:flushfail
echo TOPRGU_TRANSACTION_RELEASE_FLUSH_FAILED
exit /b 3
:dismountfail
echo TOPRGU_TRANSACTION_RELEASE_DISMOUNT_FAILED
exit /b 4
