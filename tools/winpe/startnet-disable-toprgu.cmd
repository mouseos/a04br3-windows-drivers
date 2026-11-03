@echo off
title TAB-A04BR3 MT8167 TOPRGU service recovery
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\mt8167-toprgu.sys X:\a04br3\toprgu-original.sys
if not errorlevel 1 goto matched
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\mt8167-toprgu.sys X:\a04br3\toprgu\mt8167-toprgu.sys
if errorlevel 1 goto reject
:matched
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto hivefail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto rejecthive
reg query HKLM\A04TEMP\ControlSet001\Services\Mt8167Toprgu /v Start >nul 2>&1
if errorlevel 1 goto rejecthive
reg add HKLM\A04TEMP\ControlSet001\Services\Mt8167Toprgu /v Start /t REG_DWORD /d 4 /f >nul
if errorlevel 1 goto writefail
echo TOPRGU_RECOVERY_SERVICE_DISABLED
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
goto dismount
:rejecthive
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
:reject
echo TOPRGU_RECOVERY_PRECONDITION_REJECTED
goto dismount
:writefail
echo TOPRGU_RECOVERY_WRITE_FAILED
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
goto dismount
:hivefail
echo TOPRGU_RECOVERY_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo TOPRGU_RECOVERY_HIVE_UNLOAD_FAILED
goto finish
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
echo TOPRGU_RECOVERY_FINISHED
