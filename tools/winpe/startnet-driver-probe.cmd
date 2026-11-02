@echo off
title TAB-A04-BR3 WinPE driver probe
echo Starting Windows PE device initialization...
wpeinit
echo.
echo === Attempt to load unsigned diagnostic driver ===
echo This probe rejects the adapter and cannot expose a disk.
drvload X:\a04br3\a04-msdc-probe.inf
echo drvload exit code: %errorlevel%
echo.
echo === Driver service state ===
reg query HKLM\SYSTEM\CurrentControlSet\Services\a04msdcprobe
echo.
echo === Recent device installation log ===
if exist X:\Windows\inf\setupapi.dev.log type X:\Windows\inf\setupapi.dev.log
echo.
echo Probe finished. No write commands were issued.
