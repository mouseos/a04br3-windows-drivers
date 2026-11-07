@echo off
title MT8167 BU21025 diagnostic
X:\a04br3\disable-quickedit.exe
mountvol /N
wpeinit
if errorlevel 1 goto failed
mountvol /N
if errorlevel 1 goto failed
echo MT8167_SHARED_DRVLOAD_START
drvload X:\a04br3\mt8167-shared\shared-parent.inf
if errorlevel 1 goto failed
echo BU21025_DRVLOAD_START
drvload X:\a04br3\bu21025-shared\bu21025.inf
if errorlevel 1 goto failed
echo MT8167_BU21025_DRVLOAD_RETURNED
 echo PnP and touch operation must be verified separately.
goto display
:failed
echo MT8167_BU21025_LOAD_FAILED
:display
X:\a04br3\uart-screen.exe -f
