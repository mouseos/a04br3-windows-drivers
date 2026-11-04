@echo off
title A04BR3 GE8300 141233b render diagnostic
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto failed
wpeinit
if errorlevel 1 goto failed
mountvol /N
if errorlevel 1 goto failed
set GPU=X:\a04br3\render-141233b
copy /Y %GPU%\ge8300-mips.fw X:\Windows\System32\drivers\ge8300-mips.fw >nul
if errorlevel 1 goto failed
echo RENDER_141233B_DRVLOAD_START
drvload %GPU%\mt8167-ge8300.inf
set RESULT=%errorlevel%
echo RENDER_141233B_DRVLOAD_EXIT=%RESULT%
if not "%RESULT%"=="0" goto capture
%GPU%\ge8300-pnp-status.exe
set PNP_RESULT=%errorlevel%
echo RENDER_141233B_PNP_EXIT=%PNP_RESULT%
if not "%PNP_RESULT%"=="0" goto capture
%GPU%\ge8300-status.exe
set STATUS_RESULT=%errorlevel%
echo RENDER_141233B_LIFECYCLE_STATUS_EXIT=%STATUS_RESULT%
if not "%STATUS_RESULT%"=="0" goto capture
echo RENDER_141233B_LIFECYCLE_ONLY_CHECK_UART_RENDER
echo RENDER_141233B_FINISHED_CHECK_UART
echo RENDER_141233B_SCREEN_SKIPPED
goto done
:failed
echo RENDER_141233B_PREPARATION_FAILED
:capture
rem Use the tested TOPRGU screen service only after GPU drvload has returned.
X:\a04br3\uart-screen.exe -f
echo RENDER_141233B_SCREEN_EXIT=%errorlevel%
:done
rem Screen is also available in this boot as X:\a04br3\uart-screen.exe -f.
rem No retry, no unload, no reset, no disk write.
