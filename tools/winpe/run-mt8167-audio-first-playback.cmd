@echo off
rem Run once AFTER the primary owner's signed parent/audio/TOPRGU drvload.
rem This script does not load drivers, restart devices or repeat RUN.
"%~dp0mt8167-audio-first-playback.exe"
set "A04_AUDIO_RESULT=%errorlevel%"
echo A04AUDKS_PROCESS_EXIT=%A04_AUDIO_RESULT%
exit /b %A04_AUDIO_RESULT%
