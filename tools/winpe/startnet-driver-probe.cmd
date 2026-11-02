@echo off
title TAB-A04-BR3 WinPE driver probe
echo Starting Windows PE device initialization...
wpeinit
echo.
echo === Attempt to load unsigned diagnostic driver ===
echo This probe rejects the adapter and cannot expose a disk.
drvload X:\a04br3\a04-msdc-probe.inf
set "DRIVER_EXIT=%errorlevel%"
echo drvload exit code: %DRIVER_EXIT%
echo.
echo === Driver service state ===
reg query HKLM\SYSTEM\CurrentControlSet\Services\a04msdcprobe
echo.
echo === Recent device installation log ===
if exist X:\Windows\inf\setupapi.dev.log find /i "!!!" X:\Windows\inf\setupapi.dev.log
echo.
echo drvload exit code: %DRIVER_EXIT%
echo Probe finished. No disk write commands were issued.
