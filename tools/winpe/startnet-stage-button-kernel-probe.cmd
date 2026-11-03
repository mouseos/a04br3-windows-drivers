@echo off
title TAB-A04BR3 guarded kernel button probe setup
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
if exist C:\Windows\System32\drivers\a04buttonprobe.sys goto exists
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regloadfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto regreject
reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe >nul 2>&1
if not errorlevel 1 goto regreject
copy /b X:\a04br3\button-kernel-probe.sys C:\Windows\System32\drivers\a04buttonprobe.sys
if errorlevel 1 goto rollback
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04buttonprobe.sys X:\a04br3\button-kernel-probe.sys
if errorlevel 1 goto rollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Type /t REG_DWORD /d 1 /f >nul
if errorlevel 1 goto rollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v Start /t REG_DWORD /d 1 /f >nul
if errorlevel 1 goto rollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v ErrorControl /t REG_DWORD /d 1 /f >nul
if errorlevel 1 goto rollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /v ImagePath /t REG_EXPAND_SZ /d \SystemRoot\System32\drivers\a04buttonprobe.sys /f >nul
if errorlevel 1 goto rollback
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
echo BUTTON_KERNEL_PROBE_STAGED
goto dismount
:rollback
reg delete HKLM\A04TEMP\ControlSet001\Services\A04ButtonKernelProbe /f >nul 2>&1
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
del C:\Windows\System32\drivers\a04buttonprobe.sys >nul 2>&1
echo BUTTON_KERNEL_PROBE_ROLLED_BACK
goto dismount
:regreject
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
echo BUTTON_KERNEL_PROBE_PRECONDITION_REJECTED
goto dismount
:unloadfail
echo BUTTON_KERNEL_PROBE_HIVE_UNLOAD_FAILED
goto finish
:regloadfail
echo BUTTON_KERNEL_PROBE_HIVE_LOAD_FAILED
goto dismount
:exists
echo BUTTON_KERNEL_PROBE_FILE_ALREADY_EXISTS
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
echo BUTTON_KERNEL_PROBE_SETUP_FINISHED
