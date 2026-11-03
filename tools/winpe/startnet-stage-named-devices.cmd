@echo off
title TAB-A04BR3 hardware driver staging
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04touch.sys X:\a04br3\touch-provider-original.sys
if errorlevel 1 goto reject
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\a04msdcprobe.sys X:\a04br3\storage-original.sys
if errorlevel 1 goto reject
if exist C:\Windows\System32\drivers\bu21025.sys goto reject
if exist C:\Windows\Temp\a04br3-named-touch-dism.log goto reject
if exist C:\Windows\Temp\a04br3-named-toprgu-dism.log goto reject
if exist C:\Windows\Temp\a04br3-named-storage-dism.log goto reject
X:\a04br3\read-emmc-model.exe
if errorlevel 1 goto modelfail
drvload X:\a04br3\touch-provider\bu21025.inf
if errorlevel 1 goto touchfail
X:\a04br3\touch-provider-status.exe
if errorlevel 1 goto touchfail
drvload X:\a04br3\toprgu\mt8167-toprgu.inf
if errorlevel 1 goto toprgufail
X:\a04br3\toprgu-provider-status.exe
if errorlevel 1 goto toprgufail
echo NAMED_DEVICE_WINPE_STARTED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\touch-provider\bu21025.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\named-touch.log 2>&1
set "A04_RESULT=%errorlevel%"
copy /b X:\a04br3\named-touch.log C:\Windows\Temp\a04br3-named-touch-dism.log >nul
if errorlevel 1 goto logfail
echo NAMED_TOUCH_DISM_EXIT=%A04_RESULT%
if not "%A04_RESULT%"=="0" goto dismfail
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\toprgu\mt8167-toprgu.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\named-toprgu.log 2>&1
set "A04_RESULT=%errorlevel%"
copy /b X:\a04br3\named-toprgu.log C:\Windows\Temp\a04br3-named-toprgu-dism.log >nul
if errorlevel 1 goto logfail
echo NAMED_TOPRGU_DISM_EXIT=%A04_RESULT%
if not "%A04_RESULT%"=="0" goto dismfail
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\storage-upgrade\mt8167-msdc.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\named-storage.log 2>&1
set "A04_RESULT=%errorlevel%"
copy /b X:\a04br3\named-storage.log C:\Windows\Temp\a04br3-named-storage-dism.log >nul
if errorlevel 1 goto logfail
echo NAMED_STORAGE_DISM_EXIT=%A04_RESULT%
if not "%A04_RESULT%"=="0" goto dismfail
echo NAMED_DEVICE_WINDOWS_STAGED
goto dismount
:reject
echo NAMED_DEVICE_PRECONDITION_REJECTED
goto dismount
:touchfail
echo NAMED_TOUCH_WINPE_FAILED
goto dismount
:toprgufail
echo NAMED_TOPRGU_WINPE_FAILED
goto dismount
:modelfail
echo NAMED_EMMC_MODEL_FAILED
goto dismount
:logfail
echo NAMED_DEVICE_LOG_SAVE_FAILED
goto dismount
:dismfail
echo NAMED_DEVICE_DISM_FAILED
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
echo NAMED_DEVICE_STAGING_FINISHED
