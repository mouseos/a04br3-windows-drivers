@echo off
title PowerVR GE8300 native firmware startup diagnostic
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto failed
wpeinit
if errorlevel 1 goto failed
mountvol /N
if errorlevel 1 goto failed
set "GPU=X:\a04br3\vendor-m1-11"
copy /Y "%GPU%\ge8300-mips.fw" X:\Windows\System32\drivers\ge8300-mips.fw >nul
if errorlevel 1 goto failed
echo GE8300_VENDOR_STAGE_DRVLOAD_START
drvload "%GPU%\mt8167-ge8300.inf"
if errorlevel 1 goto failed
"%GPU%\ge8300-pnp-status.exe"
if errorlevel 1 goto failed
"%GPU%\ge8300-vendor-start-status.exe"
if errorlevel 1 goto failed
echo GE8300_NATIVE_FIRMWARE_DIAGNOSTIC_PASSED
goto done
:failed
echo GE8300_VENDOR_STAGE_FAILED
X:\a04br3\uart-screen.exe -f
:done
