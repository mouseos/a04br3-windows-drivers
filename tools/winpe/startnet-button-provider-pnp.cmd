@echo off
title TAB-A04BR3 experimental button provider PnP check
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto storagefail
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto guardfail
drvload X:\a04br3\button-provider\a04-button-provider.inf
if errorlevel 1 goto providerfail
echo BUTTON_PROVIDER_DRVLOAD_OK
X:\a04br3\button-provider-status.exe
if errorlevel 1 echo BUTTON_PROVIDER_STATUS_FAILED
goto dismount
:providerfail
echo BUTTON_PROVIDER_DRVLOAD_FAILED
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
echo BUTTON_PROVIDER_PNP_CHECK_FINISHED
