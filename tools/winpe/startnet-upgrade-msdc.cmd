@echo off
title TAB-A04BR3 MT8167 MSDC driver update
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
X:\a04br3\compare-files.exe C:\Windows\System32\drivers\mt8167-msdc.sys X:\a04br3\msdc-original.sys
if errorlevel 1 goto reject
if exist C:\Windows\Temp\a04br3-msdc-update-v6-dism.log goto reject
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto hivefail
set "A04_CURRENT="
set "A04_STORAGE_PATH="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
for /f "tokens=1,2,*" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "A04_STORAGE_PATH=%%C"
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
if not "%A04_CURRENT%"=="0x1" goto reject
if /i not "%A04_STORAGE_PATH%"=="System32\drivers\mt8167-msdc.sys" goto reject
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\storage-upgrade\mt8167-msdc.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\msdc-update.log 2>&1
set "A04_RESULT=%errorlevel%"
copy /b X:\a04br3\msdc-update.log C:\Windows\Temp\a04br3-msdc-update-v6-dism.log >nul
if errorlevel 1 goto logfail
echo MSDC_UPDATE_DISM_EXIT=%A04_RESULT%
if not "%A04_RESULT%"=="0" goto dismfail
echo MSDC_UPDATE_WINDOWS_STAGED
goto dismount
:reject
echo MSDC_UPDATE_PRECONDITION_REJECTED
goto dismount
:hivefail
echo MSDC_UPDATE_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo MSDC_UPDATE_HIVE_UNLOAD_FAILED
goto finish
:logfail
echo MSDC_UPDATE_LOG_SAVE_FAILED
goto dismount
:dismfail
echo MSDC_UPDATE_DISM_FAILED
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
echo MSDC_UPDATE_FINISHED
