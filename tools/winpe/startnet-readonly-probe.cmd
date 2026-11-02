@echo off
title TAB-A04-BR3 read-only disk probe
wpeinit
echo === Load read-only diagnostic driver ===
drvload X:\a04br3\a04-msdc-probe.inf
echo drvload exit code: %errorlevel%
echo === Disk enumeration ===
(echo list disk&echo list volume&echo exit)>X:\a04br3\list-disks.txt
diskpart /s X:\a04br3\list-disks.txt
echo === Read-only diagnostic finished ===
