@echo off
rem Run after shared parent and existing TOPRGU screen UART are ready.
X:\a04br3\usb-power-uart.exe
exit /b %ERRORLEVEL%
