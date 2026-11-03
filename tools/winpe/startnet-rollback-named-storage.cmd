@echo off
title TAB-A04BR3 MT8167 MSDC boot service rollback
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04msdcprobe.sys X:\a04br3\storage-original.sys
if errorlevel 1 goto reject
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto hivefail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto rejecthive
reg query HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath >nul 2>&1
if errorlevel 1 goto rejecthive
reg add HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath /t REG_EXPAND_SZ /d "System32\drivers\a04msdcprobe.sys" /f >nul
if errorlevel 1 goto writefail
echo NAMED_STORAGE_ROLLBACK_SERVICE_RESTORED
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
goto dismount
:rejecthive
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
:reject
echo NAMED_STORAGE_ROLLBACK_REJECTED
goto dismount
:writefail
echo NAMED_STORAGE_ROLLBACK_WRITE_FAILED
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
goto dismount
:unloadfail
echo NAMED_STORAGE_ROLLBACK_HIVE_UNLOAD_FAILED
goto finish
:hivefail
echo NAMED_STORAGE_ROLLBACK_HIVE_LOAD_FAILED
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
echo NAMED_STORAGE_ROLLBACK_FINISHED
