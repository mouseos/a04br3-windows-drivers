@echo off
rem Read-only observation. Existing TOPRGU UART owner must be running.
X:\a04br3\mt6630-pnp-uart.exe
exit /b %ERRORLEVEL%
