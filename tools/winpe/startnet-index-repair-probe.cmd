@echo off
title TAB-A04BR3 guarded NTFS index and button interface probe
wpeinit
echo Loading guarded storage driver...
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto driverfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
chkdsk C: >X:\a04br3\check-before.txt 2>&1
set "A04_CHECK=%errorlevel%"
type X:\a04br3\check-before.txt
if not "%A04_CHECK%"=="3" goto unexpected
X:\a04br3\guard-windows-volume.exe --check-chkdsk
if errorlevel 1 goto unexpected
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
echo REPAIRING_KNOWN_INDEX_AFTER_BACKUP
chkdsk C: /f /x >X:\a04br3\check-repair.txt 2>&1
set "A04_REPAIR=%errorlevel%"
type X:\a04br3\check-repair.txt
if "%A04_REPAIR%"=="0" goto repairchecked
if not "%A04_REPAIR%"=="1" goto checkfail
:repairchecked
chkdsk C: >X:\a04br3\check-after.txt 2>&1
set "A04_AFTER=%errorlevel%"
type X:\a04br3\check-after.txt
if not "%A04_AFTER%"=="0" goto checkfail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
echo CHKDSK_CLEAN_AFTER_REPAIR
if exist C:\Windows\System32\a04-button-probe.exe goto probeexists
copy /y X:\a04br3\button-probe-service.exe C:\Windows\System32\a04-button-probe.exe
if errorlevel 1 goto probecopyfail
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regloadfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto regprecondition
reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe >nul 2>&1
if not errorlevel 1 goto regexists
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /v Type /t REG_DWORD /d 16 /f >nul
if errorlevel 1 goto regrollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /v Start /t REG_DWORD /d 2 /f >nul
if errorlevel 1 goto regrollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /v ErrorControl /t REG_DWORD /d 1 /f >nul
if errorlevel 1 goto regrollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /v ObjectName /t REG_SZ /d LocalSystem /f >nul
if errorlevel 1 goto regrollback
reg add HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /v ImagePath /t REG_SZ /d C:\Windows\System32\a04-button-probe.exe /f >nul
if errorlevel 1 goto regrollback
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
echo BUTTON_PROBE_SERVICE_STAGED
goto dismount
:regrollback
reg delete HKLM\A04TEMP\ControlSet001\Services\A04ButtonProbe /f >nul 2>&1
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
del C:\Windows\System32\a04-button-probe.exe
echo BUTTON_PROBE_REGISTRATION_FAILED
goto dismount
:regprecondition
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
del C:\Windows\System32\a04-button-probe.exe
echo BUTTON_PROBE_CONTROLSET_REJECTED
goto dismount
:regexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
del C:\Windows\System32\a04-button-probe.exe
echo BUTTON_PROBE_SERVICE_ALREADY_EXISTS
goto dismount
:regloadfail
del C:\Windows\System32\a04-button-probe.exe
echo BUTTON_PROBE_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo BUTTON_PROBE_HIVE_UNLOAD_FAILED
goto finish
:probecopyfail
del C:\Windows\System32\a04-button-probe.exe >nul 2>&1
echo BUTTON_PROBE_COPY_FAILED
goto dismount
:probeexists
echo BUTTON_PROBE_FILE_ALREADY_EXISTS
goto dismount
:checkfail
echo CHKDSK_NOT_CLEAN_SERVICE_NOT_STAGED
goto dismount
:repairfail
echo ROOT_REPAIR_REJECTED_OR_FAILED
if not exist C:\Windows\System32\config\SYSTEM goto finish
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
goto finish
:unexpected
echo UNEXPECTED_CHKDSK_OUTPUT_NO_REPAIR
goto dismount
:guardfail
echo VOLUME_GUARD_REJECTED_NO_REPAIR
goto finish
:driverfail
echo STORAGE_DRIVER_FAILED
:finish
echo ROOT_REPAIR_PROBE_DIAGNOSTIC_FINISHED
