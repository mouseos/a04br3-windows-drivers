@echo off
title TAB-A04BR3 experimental button provider staging
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
if exist C:\Windows\System32\drivers\a04buttonprovider.sys goto reject
if exist C:\Windows\Temp\a04br3-button-provider-dism.log goto reject
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto regfail
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
if not "%A04_CURRENT%"=="0x1" goto serviceexists
reg query HKLM\A04TEMP\ControlSet001\Services\A04ButtonProvider >nul 2>&1
if not errorlevel 1 goto serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
drvload X:\a04br3\button-provider\a04-button-provider.inf
if errorlevel 1 goto providerfail
X:\a04br3\button-provider-status.exe
if errorlevel 1 goto providerfail
echo BUTTON_PROVIDER_WINPE_STARTED
dism.exe /Image:C:\ /Add-Driver /Driver:X:\a04br3\button-provider\a04-button-provider.inf /ForceUnsigned /ScratchDir:X:\Windows\Temp > X:\a04br3\button-provider-dism.log 2>&1
set A04_DISM_RESULT=%errorlevel%
copy /b X:\a04br3\button-provider-dism.log C:\Windows\Temp\a04br3-button-provider-dism.log >nul
if errorlevel 1 goto logfail
type X:\a04br3\button-provider-dism.log
echo BUTTON_PROVIDER_DISM_EXIT=%A04_DISM_RESULT%
if not "%A04_DISM_RESULT%"=="0" goto dismfail
echo BUTTON_PROVIDER_WINDOWS_STAGED
goto dismount
:serviceexists
reg unload HKLM\A04TEMP
if errorlevel 1 goto unloadfail
:reject
echo BUTTON_PROVIDER_PRECONDITION_REJECTED
goto dismount
:regfail
echo BUTTON_PROVIDER_HIVE_LOAD_FAILED
goto dismount
:unloadfail
echo BUTTON_PROVIDER_HIVE_UNLOAD_FAILED
goto finish
:providerfail
echo BUTTON_PROVIDER_WINPE_START_FAILED
goto dismount
:logfail
echo BUTTON_PROVIDER_DISM_LOG_SAVE_FAILED
goto dismount
:dismfail
echo BUTTON_PROVIDER_DISM_FAILED
goto dismount
:guardfail
echo VOLUME_GUARD_REJECTED
goto finish
:storagefail
echo STORAGE_DRIVER_FAILED
goto finish
:dismount
mountvol C: /p
echo MOUNTVOL_EXIT=%errorlevel%
:finish
echo BUTTON_PROVIDER_STAGING_FINISHED
