@echo off
title TAB-A04BR3 guarded root attribute repair
wpeinit
echo Loading guarded storage driver...
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
echo Applying guarded root attribute repair...
X:\a04br3\repair-root.exe
if errorlevel 1 goto repairfail
echo Checking Windows volume without repair options...
chkdsk C:
echo chkdsk exit code: %errorlevel%
echo Dismounting Windows volume...
mountvol C: /p
echo mountvol exit code: %errorlevel%
goto finish
:driverfail
echo Storage driver failed; no repair attempted.
goto finish
:repairfail
echo Root repair rejected or failed. No further repair attempted.
if exist C:\Windows\System32\config\SYSTEM mountvol C: /p
:finish
echo Root attribute diagnostic finished.
