@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Offline driver update for the installed Windows RT volume: NXP MMA8652 accelerometer start diagnostics. The rollback point is the host image of the Windows partition.
set "A04_REPORT=X:\a04br3\toprgu-audit-result.txt"
set "A04_BK=C:\a04backup\mma8652-code10-v4"
> "%A04_REPORT%" echo UPDATE_BEGIN
X:\a04br3\observe-msdc-storage.exe >> "%A04_REPORT%" 2>&1
if "%errorlevel%"=="0" goto storageobserved
X:\a04br3\observe-msdc-storage.exe --verify-package >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto packagefail
drvload X:\a04br3\a04-msdc-probe.inf >> "%A04_REPORT%" 2>&1
>> "%A04_REPORT%" echo MSDC_DRVLOAD_EXIT=%errorlevel%
:storageobserved
set /a A04_MAP_ATTEMPTS=0
:mapretry
set /a A04_MAP_ATTEMPTS+=1
>> "%A04_REPORT%" echo A04MAP_ATTEMPT=%A04_MAP_ATTEMPTS%
X:\a04br3\map-offline-windows.exe >> "%A04_REPORT%" 2>&1
set "A04_MAP_RC=%errorlevel%"
>> "%A04_REPORT%" echo A04MAP_EXIT=%A04_MAP_RC%
if "%A04_MAP_RC%"=="0" goto mapped
if not "%A04_MAP_RC%"=="4" goto mapfail
if %A04_MAP_ATTEMPTS% GEQ 6 goto mapfail
X:\a04br3\observe-msdc-storage.exe --wait >> "%A04_REPORT%" 2>&1
goto mapretry
:mapped
X:\a04br3\guard-windows-volume.exe >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto guardfail
if not exist "C:\Windows\System32\config\SYSTEM" goto guardfail
if exist "%A04_BK%" goto journalexists
mkdir "%A04_BK%" >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto backupfail
> "%A04_BK%\JOURNAL.txt" echo STATE=DISM_STARTED
call :adddriver ACCEL X:\a04br3\upd\accel\mma8652.inf
if errorlevel 1 goto dismfail
copy /y X:\a04br3\dism-*.txt "%A04_BK%\" >nul
> "%A04_BK%\JOURNAL.txt" echo STATE=COMMITTED
>> "%A04_REPORT%" echo UPDATE_STATUS=COMMITTED
set "A04_RC=0"
goto release
:adddriver
dism.exe /Image:C:\ /Add-Driver /Driver:%2 /ForceUnsigned /ScratchDir:X:\Windows\Temp /LogPath:X:\a04br3\dism-%1.log > X:\a04br3\dism-%1.txt 2>&1
set "A04_DISM=%errorlevel%"
findstr /i /c:"successfully" /c:"Error" /c:"Installing" X:\a04br3\dism-%1.txt >> "%A04_REPORT%"
>> "%A04_REPORT%" echo UPDATE_DISM_%1_EXIT=%A04_DISM%
if not "%A04_DISM%"=="0" exit /b 1
exit /b 0
:dismfail
copy /y X:\a04br3\dism-*.txt "%A04_BK%\" >nul
> "%A04_BK%\JOURNAL.txt" echo STATE=DISM_FAILED_RESTORE_FROM_HOST_IMAGE
>> "%A04_REPORT%" echo UPDATE_STATUS=DISM_FAILED_RESTORE_FROM_HOST_IMAGE
set "A04_RC=6"
goto release
:backupfail
>> "%A04_REPORT%" echo UPDATE_STATUS=BACKUP_FAILED_NO_CHANGE
set "A04_RC=5"
goto release
:journalexists
type "%A04_BK%\JOURNAL.txt" >> "%A04_REPORT%" 2>&1
>> "%A04_REPORT%" echo UPDATE_STATUS=JOURNAL_EXISTS_NO_CHANGE
set "A04_RC=4"
goto release
:release
X:\a04br3\release-offline-volume.exe >> "%A04_REPORT%" 2>&1
set "A04_RELEASE_RC=%errorlevel%"
>> "%A04_REPORT%" echo UPDATE_RELEASE_EXIT=%A04_RELEASE_RC%
goto transmit
:packagefail
>> "%A04_REPORT%" echo UPDATE_STATUS=MSDC_PACKAGE_PIN_REJECTED
set "A04_RC=2"
goto transmit
:mapfail
>> "%A04_REPORT%" echo UPDATE_STATUS=VOLUME_MAPPING_REJECTED
set "A04_RC=2"
goto transmit
:guardfail
>> "%A04_REPORT%" echo UPDATE_STATUS=VOLUME_IDENTITY_REJECTED
set "A04_RC=2"
:transmit
>> "%A04_REPORT%" echo UPDATE_EXIT=%A04_RC%
X:\a04br3\uart-audit-stream.exe 981D5A3EB11CA480
exit /b %A04_RC%
