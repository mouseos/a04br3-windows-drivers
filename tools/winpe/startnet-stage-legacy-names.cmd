@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "A04_OUTCOME=FAILED"
title MT8167 MSDC and BU21025 driver names
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\check-volume-clean.exe
if errorlevel 1 goto dirtyfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04msdcprobe.sys X:\a04br3\names\storage\a04msdcprobe.sys
if errorlevel 1 goto mismatch
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04touch.sys X:\a04br3\names\touch\a04touch.sys
if errorlevel 1 goto mismatch
X:\a04br3\compare-files.exe C:\Windows\INF\oem32.inf X:\a04br3\names\original-storage.inf
if errorlevel 1 goto mismatch
X:\a04br3\compare-files.exe C:\Windows\INF\oem33.inf X:\a04br3\names\original-touch.inf
if errorlevel 1 goto mismatch
rem DISM configures services as well as staging files. Refuse to change an
rem installation whose active services point to different driver binaries.
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM >nul 2>&1
if errorlevel 1 goto mismatch
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
set "A04_STORAGE_PATH="
set "A04_TOUCH_PATH="
for /f "tokens=1,2,*" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "A04_STORAGE_PATH=%%C"
for /f "tokens=1,2,*" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\A04Touch /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "A04_TOUCH_PATH=%%C"
reg unload HKLM\A04TEMP >nul 2>&1
if errorlevel 1 goto hiveunloadfail
if not "!A04_CURRENT!"=="0x1" goto mismatch
if /i not "!A04_STORAGE_PATH!"=="System32\drivers\a04msdcprobe.sys" goto mismatch
if /i not "!A04_TOUCH_PATH!"=="\SystemRoot\System32\drivers\a04touch.sys" goto mismatch
if exist C:\Windows\Temp\mt8167-bu21025-names-stage-v2 goto reject
rem The new directory is a persistent one-shot guard and partial hive backup.
mkdir C:\Windows\Temp\mt8167-bu21025-names-stage-v2
if errorlevel 1 goto reject
for %%H in (SYSTEM SOFTWARE COMPONENTS) do (
  if exist C:\Windows\System32\config\%%H (
    X:\a04br3\copy-backup.exe C:\Windows\System32\config\%%H C:\Windows\Temp\mt8167-bu21025-names-stage-v2\%%H
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H C:\Windows\Temp\mt8167-bu21025-names-stage-v2\%%H
    if errorlevel 1 goto backupfail
  ) else (
    if /i not "%%H"=="COMPONENTS" goto backupfail
  )
  for %%E in (.LOG1 .LOG2) do if exist C:\Windows\System32\config\%%H%%E (
    X:\a04br3\copy-backup.exe C:\Windows\System32\config\%%H%%E C:\Windows\Temp\mt8167-bu21025-names-stage-v2\%%H%%E
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H%%E C:\Windows\Temp\mt8167-bu21025-names-stage-v2\%%H%%E
    if errorlevel 1 goto backupfail
  )
)
rem The guards above check the currently selected service binaries. Offline
rem staging alone does not prove that PnP selected the new descriptions.
X:\a04br3\uart-name-event.exe START
if errorlevel 1 goto uartfail
for %%D in (storage touch) do (
  dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\names\%%D /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\names-%%D.log 2>&1
  set "A04_RESULT=!errorlevel!"
  X:\a04br3\copy-backup.exe X:\a04br3\names-%%D.log C:\Windows\Temp\mt8167-bu21025-names-stage-v2\%%D.log
  if errorlevel 1 goto logfail
  type X:\a04br3\names-%%D.log
  echo NAMES_%%D_DISM_EXIT=!A04_RESULT!
  if not "!A04_RESULT!"=="0" goto dismfail
)
echo NAMES_PACKAGES_STAGED_PNP_NOT_VERIFIED
set "A04_OUTCOME=STAGED"
goto dismount
:consolefail
echo NAMES_CONSOLE_FAILED
goto finish
:storagefail
echo NAMES_STORAGE_FAILED
goto finish
:guardfail
echo NAMES_VOLUME_GUARD_FAILED
goto finish
:dirtyfail
echo NAMES_VOLUME_NOT_CLEAN
goto finish
:mismatch
echo NAMES_INSTALLED_PACKAGE_MISMATCH
goto dismount
:hiveunloadfail
echo NAMES_HIVE_UNLOAD_FAILED
set "A04_OUTCOME=UNSAFE"
goto finish
:reject
echo NAMES_ALREADY_ATTEMPTED_OR_LOCK_FAILED
goto dismount
:backupfail
echo NAMES_HIVE_BACKUP_FAILED
goto dismount
:logfail
echo NAMES_LOG_COPY_FAILED
set "A04_OUTCOME=PARTIAL"
goto dismount
:dismfail
echo NAMES_DISM_FAILED_MAY_BE_PARTIAL
set "A04_OUTCOME=PARTIAL"
goto dismount
:uartfail
echo NAMES_UART_EVENT_FAILED
:dismount
mountvol C: /p
echo NAMES_MOUNTVOL_EXIT=!errorlevel!
if errorlevel 1 set "A04_OUTCOME=UNSAFE"
:finish
X:\a04br3\uart-name-event.exe !A04_OUTCOME!
echo NAMES_STAGE_FINISHED
