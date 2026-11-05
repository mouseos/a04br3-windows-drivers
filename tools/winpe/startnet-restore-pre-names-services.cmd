@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "A04_OUTCOME=FAILED"
title MT8167 MSDC and BU21025 service restoration
X:\a04br3\disable-quickedit.exe
if errorlevel 1 goto finish
wpeinit
drvload X:\a04br3\a04-msdc-probe.inf
if errorlevel 1 goto finish
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto finish
X:\a04br3\check-volume-clean.exe
if errorlevel 1 goto dismount
for %%F in (mt8167-msdc.sys bu21025.sys a04msdcprobe.sys a04touch.sys) do (
  X:\a04br3\compare-files.exe C:\Windows\System32\drivers\%%F X:\a04br3\restore-names\%%F
  if errorlevel 1 goto dismount
)
if exist C:\Windows\Temp\mt8167-bu21025-service-restore goto dismount
mkdir C:\Windows\Temp\mt8167-bu21025-service-restore
if errorlevel 1 goto dismount
for %%F in (SYSTEM SYSTEM.LOG1 SYSTEM.LOG2) do if exist C:\Windows\System32\config\%%F (
  X:\a04br3\copy-backup.exe C:\Windows\System32\config\%%F C:\Windows\Temp\mt8167-bu21025-service-restore\%%F
  if errorlevel 1 goto dismount
  X:\a04br3\compare-files.exe C:\Windows\System32\config\%%F C:\Windows\Temp\mt8167-bu21025-service-restore\%%F
  if errorlevel 1 goto dismount
)
reg load HKLM\A04TEMP C:\Windows\System32\config\SYSTEM
if errorlevel 1 goto dismount
set "CUR="
set "STORAGE="
set "TOUCH="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TEMP\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "CUR=%%C"
for /f "tokens=1,2,*" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "STORAGE=%%C"
for /f "tokens=1,2,*" %%A in ('reg query HKLM\A04TEMP\ControlSet001\Services\A04Touch /v ImagePath 2^>nul') do if /i "%%A"=="ImagePath" set "TOUCH=%%C"
if not "!CUR!"=="0x1" goto unload
if /i not "!STORAGE!"=="System32\drivers\a04msdcprobe.sys" goto unload
if /i not "!TOUCH!"=="\SystemRoot\System32\drivers\a04touch.sys" goto unload
reg add HKLM\A04TEMP\ControlSet001\Services\a04msdcboot /v ImagePath /t REG_EXPAND_SZ /d "System32\drivers\mt8167-msdc.sys" /f
if errorlevel 1 goto unload
set "A04_OUTCOME=PARTIAL"
reg add HKLM\A04TEMP\ControlSet001\Services\A04Touch /v ImagePath /t REG_EXPAND_SZ /d "\SystemRoot\System32\drivers\bu21025.sys" /f
if errorlevel 1 goto unload
set "A04_OUTCOME=STAGED"
:unload
reg unload HKLM\A04TEMP
if errorlevel 1 set "A04_OUTCOME=UNSAFE"
if "!A04_OUTCOME!"=="UNSAFE" goto finish
:dismount
mountvol C: /p
if errorlevel 1 set "A04_OUTCOME=UNSAFE"
:finish
X:\a04br3\uart-name-event.exe !A04_OUTCOME!
echo SERVICE_RESTORE_!A04_OUTCOME!
