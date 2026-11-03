@echo off
title TAB-A04BR3 guarded kernel button probe removal
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
if not exist C:\Windows\System32\drivers\a04buttonprobe.sys goto mismatch
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04buttonprobe.sys X:\a04br3\button-kernel-probe.sys
if errorlevel 1 goto mismatch
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regloadfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto regreject
reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe >nul 2>&1
if errorlevel 1 goto regreject
reg delete HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /f >nul
if errorlevel 1 goto regreject
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
del C:\Windows\System32\drivers\a04buttonprobe.sys
if errorlevel 1 goto deletefail
echo BUTTON_KERNEL_PROBE_REMOVED
goto dismount
:regreject
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
echo BUTTON_KERNEL_PROBE_REGISTRY_REJECTED
goto dismount
:unloadfail
echo BUTTON_KERNEL_PROBE_HIVE_UNLOAD_FAILED
goto finish
:regloadfail
echo BUTTON_KERNEL_PROBE_HIVE_LOAD_FAILED
goto dismount
:deletefail
echo BUTTON_KERNEL_PROBE_FILE_DELETE_FAILED
goto dismount
:mismatch
echo BUTTON_KERNEL_PROBE_FILE_MISMATCH
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
echo BUTTON_KERNEL_PROBE_REMOVAL_FINISHED
