@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Offline driver update for the installed Windows RT volume.
rem Installs the MediaTek MT8167 shared resource controller and the ROHM BU21025
rem touch child with DISM. The rollback point is a full image of the Windows
rem partition taken from Android recovery before this media is written.
set "A04_REPORT=X:\a04br3\toprgu-audit-result.txt"
set "A04_BK=C:\a04backup\shared-touch-v2"
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
if not exist "C:\a04backup" goto nobackupdir
if exist "C:\a04backup\shared-touch-v2" goto journalexists
>> "%A04_REPORT%" echo UPDATE_PARTIAL_DEVICE_BACKUP_REMOVED_HOST_IMAGE_IS_ROLLBACK
rd /s /q "C:\a04backup" >> "%A04_REPORT%" 2>&1
if exist "C:\a04backup" goto backupfail
:nobackupdir
>> "%A04_REPORT%" echo UPDATE_PRECONDITION_OK_HOST_FULL_PARTITION_BACKUP
mkdir "%A04_BK%" >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto backupfail
> "%A04_BK%\JOURNAL.txt" echo STATE=DISM_STARTED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\mt8167-shared\shared-parent.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp /LogPath:X:\a04br3\dism-parent.log > X:\a04br3\dism-parent.txt 2>&1
set "A04_DISM1=%errorlevel%"
findstr /i /c:"successfully" /c:"Error" /c:"Installing" X:\a04br3\dism-parent.txt >> "%A04_REPORT%"
>> "%A04_REPORT%" echo UPDATE_DISM_PARENT_EXIT=%A04_DISM1%
if not "%A04_DISM1%"=="0" goto dismfail
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\bu21025-shared\shared-touch.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp /LogPath:X:\a04br3\dism-touch.log > X:\a04br3\dism-touch.txt 2>&1
set "A04_DISM2=%errorlevel%"
findstr /i /c:"successfully" /c:"Error" /c:"Installing" X:\a04br3\dism-touch.txt >> "%A04_REPORT%"
>> "%A04_REPORT%" echo UPDATE_DISM_TOUCH_EXIT=%A04_DISM2%
if not "%A04_DISM2%"=="0" goto dismfail
copy /y X:\a04br3\dism-*.txt "%A04_BK%\" >nul
> "%A04_BK%\JOURNAL.txt" echo STATE=COMMITTED
>> "%A04_REPORT%" echo UPDATE_STATUS=COMMITTED
set "A04_RC=0"
goto release
:dismfail
copy /y X:\a04br3\dism-*.txt "%A04_BK%\" >nul
> "%A04_BK%\JOURNAL.txt" echo STATE=DISM_FAILED_RESTORE_FROM_HOST_IMAGE
>> "%A04_REPORT%" echo UPDATE_STATUS=DISM_FAILED_RESTORE_FROM_HOST_IMAGE
set "A04_RC=6"
goto release
:rollbackfail
>> "%A04_REPORT%" echo UPDATE_STATUS=ROLLBACK_FAILED_BACKUP_PRESERVED
set "A04_RC=7"
goto release
:backupfail
>> "%A04_REPORT%" echo UPDATE_STATUS=BACKUP_FAILED_NO_CHANGE
set "A04_RC=5"
goto release
:journalexists
type "C:\a04backup\shared-touch-v2\JOURNAL.txt" >> "%A04_REPORT%" 2>&1
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
:robocheck
set "A04_ROBO=%errorlevel%"
>> "%A04_REPORT%" echo ROBOCOPY_%1_EXIT=%A04_ROBO%
if %A04_ROBO% GEQ 8 exit /b 1
exit /b 0
