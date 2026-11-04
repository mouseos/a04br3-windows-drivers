@echo off
title A04BR3 GE8300 09c630d render diagnostic
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto failed
wpeinit
if errorlevel 1 goto failed
mountvol /N
if errorlevel 1 goto failed
set GPU=X:\a04br3\render-09c630d
copy /Y %GPU%\ge8300-mips.fw X:\Windows\System32\drivers\ge8300-mips.fw >nul
if errorlevel 1 goto failed
echo RENDER_751DF54_DRVLOAD_START
drvload %GPU%\mt8167-ge8300.inf
set RESULT=%errorlevel%
echo RENDER_751DF54_DRVLOAD_EXIT=%RESULT%
if not "%RESULT%"=="0" goto capture
%GPU%\ge8300-pnp-status.exe
echo RENDER_751DF54_PNP_EXIT=%errorlevel%
%GPU%\ge8300-status.exe
echo RENDER_751DF54_STATUS_EXIT=%errorlevel%
echo RENDER_751DF54_FINISHED_CHECK_UART
goto capture
:failed
echo RENDER_751DF54_PREPARATION_FAILED
:capture
rem Use the tested TOPRGU screen service only after GPU drvload has returned.
X:\a04br3\uart-screen.exe -f
echo RENDER_751DF54_SCREEN_EXIT=%errorlevel%
rem No retry, no unload, no reset, no disk write.
