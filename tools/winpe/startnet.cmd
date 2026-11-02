@echo off
title TAB-A04-BR3 WinPE storage diagnostics
echo Starting Windows PE device initialization...
wpeinit
echo.
echo === Read-only disk and volume inventory ===
echo list disk > X:\a04br3-diskpart.txt
echo list volume >> X:\a04br3-diskpart.txt
diskpart /s X:\a04br3-diskpart.txt
echo.
echo === Storage service state ===
reg query HKLM\SYSTEM\CurrentControlSet\Services\sdbus /v Start
reg query HKLM\SYSTEM\CurrentControlSet\Services\sdstor /v Start
echo.
echo Diagnostics finished. This image does not modify disk partitions.
