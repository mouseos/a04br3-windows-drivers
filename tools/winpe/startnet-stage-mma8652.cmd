@echo off
setlocal EnableExtensions EnableDelayedExpansion
title TAB-A04-BR3 MMA8652 offline staging
rem This is a one-shot experimental WinPE operation, not a rollback.
wpeinit
if errorlevel 1 goto initfail
cls
echo MMA8652_OFFLINE_STAGING_STARTED
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
rem Persistent one-shot lock precedes every path that may display completion.
rem A reboot of this same WIM must never launch DISM for a second time.
if exist C:\Windows\Temp\a04br3-mma8652-servicing-lock goto finish
mkdir C:\Windows\Temp\a04br3-mma8652-servicing-lock
if errorlevel 1 goto finish
rem Require the exact package used in the prior WinPE test, supplied separately.
for %%F in (mma8652.inf mma8652.sys mma8652.cat) do (
  X:\a04br3\compare-files.exe X:\a04br3\mma8652\%%F X:\a04br3\mma8652-tested\%%F
  if errorlevel 1 goto reject
)
rem Confirm this boot has the matching sensor and real data before any writes.
drvload X:\a04br3\mma8652\mma8652.inf
if errorlevel 1 goto providerfail
X:\a04br3\mma8652\mma8652-status.exe
if errorlevel 1 goto providerfail
X:\a04br3\mma8652\mma8652-read.exe
if errorlevel 1 goto providerfail
rem Recheck the volume after the live test and immediately before modifications.
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\check-volume-clean.exe
if errorlevel 1 goto dirtyfail
if exist X:\a04br3\mma8652-services-before.txt goto reject
if exist X:\a04br3\mma8652-dism.log goto reject
if exist C:\Windows\System32\drivers\mma8652.sys goto reject
if exist C:\Windows\System32\DriverStore\FileRepository\mma8652.inf_* goto reject
if exist C:\Windows\Temp\a04br3-mma8652-preinstall goto reject
if exist C:\Windows\Temp\a04br3-mma8652-dism.log goto reject
rem Preserve hives before reg load or DISM; this is not a full-volume backup.
mkdir C:\Windows\Temp\a04br3-mma8652-preinstall
if errorlevel 1 goto backupfail
for %%H in (SYSTEM SOFTWARE COMPONENTS) do (
  if exist C:\Windows\System32\config\%%H (
    copy /b C:\Windows\System32\config\%%H C:\Windows\Temp\a04br3-mma8652-preinstall\%%H >nul
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H C:\Windows\Temp\a04br3-mma8652-preinstall\%%H
    if errorlevel 1 goto backupfail
  ) else (
    if /i not "%%H"=="COMPONENTS" goto backupfail
  )
  for %%E in (.LOG1 .LOG2) do if exist C:\Windows\System32\config\%%H%%E (
    copy /b C:\Windows\System32\config\%%H%%E C:\Windows\Temp\a04br3-mma8652-preinstall\%%H%%E >nul
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H%%E C:\Windows\Temp\a04br3-mma8652-preinstall\%%H%%E
    if errorlevel 1 goto backupfail
  )
)
rem Stage read-only verification tools in the newly created backup directory.
for %%F in (mma8652-status.exe mma8652-read.exe verify-installed.cmd) do (
  copy /b X:\a04br3\mma8652\%%F C:\Windows\Temp\a04br3-mma8652-preinstall\%%F >nul
  if errorlevel 1 goto backupfail
  X:\a04br3\compare-files.exe X:\a04br3\mma8652\%%F C:\Windows\Temp\a04br3-mma8652-preinstall\%%F
  if errorlevel 1 goto backupfail
)
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto hivefail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "!A04_CURRENT!"=="0x1" goto serviceexists
reg query HKLM\A04TEMP\ControlSet001\Services > X:\a04br3\mma8652-services-before.txt 2>&1
if errorlevel 1 goto hivequeryfail
findstr /i /x /c:"HKEY_LOCAL_MACHINE\A04TEMP\ControlSet001\Services\A04Mma8652" X:\a04br3\mma8652-services-before.txt >nul
if !errorlevel! GEQ 2 goto hivequeryfail
if not errorlevel 1 goto serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
rem DISM may partially change DriverStore even on failure; never auto-delete.
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\mma8652\mma8652.inf /ScratchDir:X:\Windows\Temp > X:\a04br3\mma8652-dism.log 2>&1
set "A04_DISM_RESULT=!errorlevel!"
copy /b X:\a04br3\mma8652-dism.log C:\Windows\Temp\a04br3-mma8652-dism.log >nul
if errorlevel 1 goto logfail
type X:\a04br3\mma8652-dism.log
echo MMA8652_DISM_EXIT=!A04_DISM_RESULT!
if not "!A04_DISM_RESULT!"=="0" goto dismfail
set /a A04_MATCHES=0
set "A04_DIR="
for /d %%D in (C:\Windows\System32\DriverStore\FileRepository\mma8652.inf_*) do (
  set /a A04_MATCHES+=1
  set "A04_DIR=%%D"
)
if not "!A04_MATCHES!"=="1" goto verifyfail
for %%F in (mma8652.inf mma8652.sys mma8652.cat) do (
  X:\a04br3\compare-files.exe "!A04_DIR!\%%F" X:\a04br3\mma8652-tested\%%F
  if errorlevel 1 goto verifyfail
)
echo MMA8652_OFFLINE_PACKAGE_VERIFIED
rem This verifies staging only, not Windows boot or Sensor API operation.
goto dismount
:hivequeryfail
echo MMA8652_HIVE_QUERY_FAILED
:serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
:reject
echo MMA8652_PRECONDITION_REJECTED
goto dismount
:providerfail
echo MMA8652_WINPE_VALIDATION_FAILED
goto dismount
:dirtyfail
echo MMA8652_VOLUME_DIRTY_OR_UNKNOWN_NO_DISM
goto dismount
:backupfail
echo MMA8652_BACKUP_FAILED_NO_DISM
goto dismount
:logfail
echo MMA8652_LOG_SAVE_FAILED_CHECK_PARTIAL_DISM
goto dismount
:dismfail
echo MMA8652_DISM_FAILED_CHECK_PARTIAL_CHANGES
goto dismount
:verifyfail
echo MMA8652_STAGING_UNVERIFIED_CHECK_PARTIAL_CHANGES
goto dismount
:hivefail
echo MMA8652_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo MMA8652_HIVE_UNLOAD_FAILED
goto finish
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:storagefail
echo STORAGE_DRIVER_FAILED
goto finish
:initfail
echo WINPE_INIT_FAILED
goto finish
:dismount
mountvol C: /p
if errorlevel 1 goto dismountfail
rem Only a successful dismount may produce this pair. Clear stale console text.
cls
echo MOUNTVOL_EXIT=0
echo MMA8652_OFFLINE_STAGING_FINISHED
rem Display a one-time image-bound QR only after both conditions.
start "" X:\a04br3\mma8652\mma8652-signal.exe
goto done
:dismountfail
echo MMA8652_DISMOUNT_FAILED_NO_RESTORE
:finish
echo MMA8652_OFFLINE_STAGING_INCOMPLETE
:done
endlocal
