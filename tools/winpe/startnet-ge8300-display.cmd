@echo off
title MT8167 Windows RT display and GE8300 test
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
mountvol /N
if errorlevel 1 goto automountfail
X:\a04br3\toprgu-provider-status.exe
if not errorlevel 1 goto watchdogready
drvload X:\a04br3\toprgu\mt8167-toprgu.inf
if errorlevel 1 goto watchdogfail
X:\a04br3\toprgu-provider-status.exe
if errorlevel 1 goto watchdogfail
:watchdogready
set GPU=X:\a04br3\ge8300-display\ge8300
set DISP=X:\a04br3\ge8300-display\display
copy /Y %GPU%\ge8300-mips.fw X:\Windows\System32\drivers\ge8300-mips.fw >nul
if errorlevel 1 goto packagefail
drvload %GPU%\mt8167-ge8300.inf
if errorlevel 1 goto gpufail
%GPU%\ge8300-pnp-status.exe
if errorlevel 1 goto gpufail
%GPU%\ge8300-service-status.exe
if errorlevel 1 goto gpufail
drvload %DISP%\mt8167-display.inf
if errorlevel 1 goto displayfail
%GPU%\display-pnp-status.exe
if errorlevel 1 goto displayfail
rem Force a full-screen update before checking the GPU copy counter.
cls
echo Windows RT MT8167 display and GE8300 combined test
echo Display PnP started. Checking completed GPU copies...
%GPU%\ge8300-copy-status.exe
if errorlevel 1 goto copyfail
echo DISPLAY_GE8300_GPU_COPY_OBSERVED
goto finish
:consolefail
echo DISPLAY_CONSOLE_FAILED
goto finish
:automountfail
echo DISPLAY_AUTOMOUNT_DISABLE_FAILED
goto finish
:watchdogfail
echo DISPLAY_WATCHDOG_FAILED
goto finish
:packagefail
echo DISPLAY_PACKAGE_FAILED
goto finish
:gpufail
echo DISPLAY_GPU_SERVICE_FAILED
goto finish
:displayfail
echo DISPLAY_PNP_FAILED
goto finish
:copyfail
echo DISPLAY_GPU_COPY_NOT_OBSERVED
:finish
echo DISPLAY_TEST_FINISHED
