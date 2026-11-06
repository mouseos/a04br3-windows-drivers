@echo off
setlocal
if not exist "%~dp0mt6630-wlan-probe.exe" (
  echo A04WLAN_LAUNCH STATUS=2
  exit /b 2
)
pushd "%~dp0" >nul 2>&1
if errorlevel 1 (
  echo A04WLAN_LAUNCH STATUS=3
  exit /b 3
)
echo A04WLAN_LAUNCH STATUS=0
"%~dp0mt6630-wlan-probe.exe" 2>nul
set "probeStatus=%errorlevel%"
popd >nul 2>&1
echo A04WLAN_LAUNCH EXIT=%probeStatus%
exit /b %probeStatus%
