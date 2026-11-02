@echo off
title TAB-A04BR3 guarded storage write diagnostic
wpeinit
echo Loading experimental storage driver...
drvload X:\a04br3\a04-msdc-probe.inf
echo drvload exit code: %errorlevel%
echo Running one-sector guarded diagnostic...
X:\a04br3\guarded-write-test.exe
echo guarded diagnostic exit code: %errorlevel%
echo Diagnostic finished.
