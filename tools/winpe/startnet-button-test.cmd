@echo off
title TAB-A04BR3 Windows button injection diagnostic
wpeinit
echo Loading original Windows RT GPIO button driver...
drvload X:\a04br3\button-driver\msgpiowin32.inf
echo GPIO driver load exit code: %errorlevel%
echo Sending one Windows button down/up pair...
X:\a04br3\button-inject.exe 1
echo Button notification diagnostic exit code: %errorlevel%
echo Notification acceptance is not proof of physical button or wake support.
