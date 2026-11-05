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
if exist C:\Windows\Temp\mt8167-bu21025-names-stage goto reject
rem The new directory is a persistent one-shot guard and partial hive backup.
mkdir C:\Windows\Temp\mt8167-bu21025-names-stage
if errorlevel 1 goto reject
for %%H in (SYSTEM SOFTWARE COMPONENTS) do (
  if exist C:\Windows\System32\config\%%H (
    copy /b C:\Windows\System32\config\%%H C:\Windows\Temp\mt8167-bu21025-names-stage\%%H >nul
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H C:\Windows\Temp\mt8167-bu21025-names-stage\%%H
    if errorlevel 1 goto backupfail
  ) else (
    if /i not "%%H"=="COMPONENTS" goto backupfail
  )
  for %%E in (.LOG1 .LOG2) do if exist C:\Windows\System32\config\%%H%%E (
    copy /b C:\Windows\System32\config\%%H%%E C:\Windows\Temp\mt8167-bu21025-names-stage\%%H%%E >nul
    if errorlevel 1 goto backupfail
    X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H%%E C:\Windows\Temp\mt8167-bu21025-names-stage\%%H%%E
    if errorlevel 1 goto backupfail
  )
)
rem The packages preserve the original SYS and service configuration. Offline
rem staging alone does not prove that PnP selected the new descriptions.
X:\a04br3\uart-name-event.exe START
if errorlevel 1 goto uartfail
for %%D in (storage touch) do (
  dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\names\%%D /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\names-%%D.log 2>&1
  set "A04_RESULT=!errorlevel!"
  copy /b X:\a04br3\names-%%D.log C:\Windows\Temp\mt8167-bu21025-names-stage\%%D.log >nul
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
:reject
echo NAMES_ALREADY_ATTEMPTED_OR_LOCK_FAILED
goto dismount
:backupfail
echo NAMES_HIVE_BACKUP_FAILED
goto dismount
:logfail
echo NAMES_LOG_COPY_FAILED
goto dismount
:dismfail
echo NAMES_DISM_FAILED_MAY_BE_PARTIAL
goto dismount
:uartfail
echo NAMES_UART_EVENT_FAILED
:dismount
mountvol C: /p
echo NAMES_MOUNTVOL_EXIT=!errorlevel!
if errorlevel 1 set "A04_OUTCOME=FAILED"
:finish
X:\a04br3\uart-name-event.exe !A04_OUTCOME!
echo NAMES_STAGE_FINISHED
