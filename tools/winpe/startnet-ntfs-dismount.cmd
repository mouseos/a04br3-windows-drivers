@echo off
title TAB-A04BR3 native NTFS mount diagnostic
wpeinit
echo Loading guarded experimental storage driver...
drvload X:\a04br3\a04-msdc-probe.inf
echo drvload exit code: %errorlevel%
(echo list disk&echo list volume&echo exit)>X:\a04br3\list-volumes.txt
diskpart /s X:\a04br3\list-volumes.txt
if not exist C:\Windows\System32\config\SYSTEM goto notwindows
echo Windows marker exists on C:; checking without repair options...
chkdsk C:
echo chkdsk exit code: %errorlevel%
echo Dismounting C: with Windows mount manager...
mountvol C: /p
echo mountvol exit code: %errorlevel%
goto finish
:notwindows
echo Expected Windows marker missing on C:; no chkdsk or dismount attempted.
:finish
echo Native NTFS diagnostic finished.
