@echo off
title GE8300 repeat and BQ cold diagnostic
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
echo BQ_COLD_DRVLOAD_START
drvload X:\a04br3\lc709203f\lc709203f.inf
echo BQ_COLD_DRVLOAD_EXIT=%errorlevel%
if errorlevel 1 goto bqfailed
X:\a04br3\lc709203f\lc709203f-pnp-status.exe
if errorlevel 1 goto bqfailed
X:\a04br3\lc709203f\lc709203f-status.exe
if errorlevel 1 goto bqfailed
echo BQ_COLD_BATTERY_QUERY_OK_UART_REQUIRED
goto done
:bqfailed
echo BQ_COLD_FAILED_CHECK_SAME_BOOT_UART
goto done
:failed
echo GE8300_VENDOR_STAGE_FAILED_BQ_SKIPPED
X:\a04br3\uart-screen.exe -f
:done
echo GE8300_BQ_DIAGNOSTIC_DONE_NO_SHUTDOWN
