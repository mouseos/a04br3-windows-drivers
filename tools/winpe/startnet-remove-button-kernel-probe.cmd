@echo off
title TAB-A04BR3 guarded kernel button probe removal
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
set "A04_FILE_PRESENT=0"
if not exist C:\Windows\System32\drivers\a04buttonprobe.sys goto checkregistry
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04buttonprobe.sys X:\a04br3\button-kernel-probe.sys
if errorlevel 1 goto mismatch
set "A04_FILE_PRESENT=1"
:checkregistry
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regloadfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto regreject
reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe >nul 2>&1
if errorlevel 1 goto regreject
set "A04_TYPE="
set "A04_START="
set "A04_IMAGE="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Type 2^>nul') do if /i "%%A"=="Type" set "A04_TYPE=%%C"
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Start 2^>nul') do if /i "%%A"=="Start" set "A04_START=%%C"
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "A04_IMAGE=%%C"
if not "%A04_TYPE%"=="0x1" goto regreject
if "%A04_START%"=="0x1" goto startchecked
if not "%A04_START%"=="0x4" goto regreject
:startchecked
if /i not "%A04_IMAGE%"=="\SystemRoot\System32\drivers\a04buttonprobe.sys" goto regreject
reg delete HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /f >nul
if errorlevel 1 goto regreject
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
if "%A04_FILE_PRESENT%"=="0" goto removed
del C:\Windows\System32\drivers\a04buttonprobe.sys
if errorlevel 1 goto deletefail
:removed
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
