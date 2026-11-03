@echo off
title MT8167 GE8300 firmware test
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto consolefail
wpeinit
drvload X:\a04br3\ge8300\mt8167-ge8300.inf
echo GE8300_DRVLOAD_EXIT=%errorlevel%
X:\a04br3\ge8300\ge8300-pnp-status.exe
echo GE8300_PNP_EXIT=%errorlevel%
X:\a04br3\ge8300\ge8300-status.exe
echo GE8300_STATUS_EXIT=%errorlevel%
echo GE8300_TEST_FINISHED
goto end
:consolefail
echo GE8300_CONSOLE_FAILED
:end
