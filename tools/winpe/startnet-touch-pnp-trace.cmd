@echo off
title TAB-A04BR3 experimental touch trace
wpeinit
drvload X:\a04br3\touch-provider\bu21025.inf
if errorlevel 1 goto failed
echo TOUCH_TRACE_DRVLOAD_OK
X:\a04br3\touch-provider-status.exe
if errorlevel 1 goto failed
echo TOUCH_TRACE_STARTED
goto finish
:failed
echo TOUCH_TRACE_FAILED
:finish
echo TOUCH_TRACE_READY
