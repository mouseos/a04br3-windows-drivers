@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Only used after a guarded audit. Revalidate identity and inspect the live
rem HKLM namespace before dismount; this never resets and never runs DISM.
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto rejected
if not exist X:\a04br3\toprgu-audit goto rejected
reg query HKLM > X:\a04br3\toprgu-audit\release-hklm.txt 2>&1
if errorlevel 1 goto registryfail
findstr /l /i /c:"HKEY_LOCAL_MACHINE\A04TOPRGUSYS" X:\a04br3\toprgu-audit\release-hklm.txt >nul
if errorlevel 2 goto registryfail
if not errorlevel 1 goto hiveloaded
findstr /l /i /c:"HKEY_LOCAL_MACHINE\A04TOPRGUSW" X:\a04br3\toprgu-audit\release-hklm.txt >nul
if errorlevel 2 goto registryfail
if not errorlevel 1 goto hiveloaded
mountvol C: /p
set "A04_MOUNTVOL_RC=%errorlevel%"
echo MOUNTVOL_EXIT=%A04_MOUNTVOL_RC%
if not "%A04_MOUNTVOL_RC%"=="0" goto mountfail
echo TOPRGU_VOLUME_RELEASED
exit /b 0
:hiveloaded
echo TOPRGU_RELEASE_HIVE_STILL_LOADED
exit /b 3
:registryfail
echo TOPRGU_RELEASE_REGISTRY_UNKNOWN
exit /b 3
:rejected
echo TOPRGU_RELEASE_GUARD_REJECTED
exit /b 3
:mountfail
echo TOPRGU_RELEASE_MOUNTVOL_FAILED
exit /b 3
