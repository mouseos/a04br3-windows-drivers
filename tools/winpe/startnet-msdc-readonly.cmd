@echo off
title TAB-A04BR3 MT8167 MSDC read-only test
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
drvload X:\a04br3\storage-readonly\mt8167-msdc-readonly.inf
if errorlevel 1 goto storagefail
echo MSDC_READONLY_DRVLOAD_OK
diskpart /s X:\a04br3\list-disks.txt
echo MSDC_READONLY_DISKPART_EXIT=%errorlevel%
if errorlevel 1 goto finish
X:\a04br3\msdc-read-stress.exe
echo MSDC_READ_STRESS_EXIT=%errorlevel%
goto finish
:consolefail
echo MSDC_READONLY_CONSOLE_FAILED
goto finish
:automountfail
echo MSDC_READONLY_AUTOMOUNT_DISABLE_FAILED
goto finish
:watchdogfail
echo MSDC_READONLY_WATCHDOG_FAILED
goto finish
:storagefail
echo MSDC_READONLY_DRVLOAD_FAILED
:finish
echo MSDC_READONLY_FINISHED
