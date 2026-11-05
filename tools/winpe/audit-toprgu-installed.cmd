@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Read-only target audit. C: must already be mounted and guard must be staged.
rem Only RAM-disk X: copies are loaded into the registry; no target hive is loaded.
rem Pinned to the test certificate in the accompanying 7109a90 manifest.
set "A04_CERT=5FC3D0043DE4A471F738FB923FEC87336E81D43C"
if not exist X:\a04br3\guard-windows-volume.exe goto invalid
X:\a04br3\guard-windows-volume.exe
if errorlevel 1 goto rejected
if not exist X:\a04br3\compare-files.exe goto failed
if not exist X:\a04br3\audit-hive-uncached.exe goto failed
if not exist X:\a04br3\toprgu-audit-candidate.sys goto failed
if exist X:\a04br3\toprgu-audit goto failed
reg query HKLM\A04TOPRGUSYS >nul 2>&1
if not errorlevel 1 goto failed
reg query HKLM\A04TOPRGUSW >nul 2>&1
if not errorlevel 1 goto failed
mkdir X:\a04br3\toprgu-audit
if errorlevel 1 goto failed
copy /b C:\Windows\System32\config\SYSTEM X:\a04br3\toprgu-audit\SYSTEM >nul
if errorlevel 1 goto failed
X:\a04br3\compare-files.exe C:\Windows\System32\config\SYSTEM X:\a04br3\toprgu-audit\SYSTEM
if errorlevel 1 goto failed
echo TOPRGU_HIVE_VERIFY=SYSTEM
X:\a04br3\audit-hive-uncached.exe SYSTEM
if errorlevel 1 goto failed
copy /b C:\Windows\System32\config\SOFTWARE X:\a04br3\toprgu-audit\SOFTWARE >nul
if errorlevel 1 goto failed
X:\a04br3\compare-files.exe C:\Windows\System32\config\SOFTWARE X:\a04br3\toprgu-audit\SOFTWARE
if errorlevel 1 goto failed
echo TOPRGU_HIVE_VERIFY=SOFTWARE
X:\a04br3\audit-hive-uncached.exe SOFTWARE
if errorlevel 1 goto failed
rem Recovery logs belong next to the copied hive. Never load or replay C:.
for %%H in (SYSTEM SOFTWARE) do for %%L in (LOG LOG1 LOG2) do if exist C:\Windows\System32\config\%%H.%%L (
  copy /b C:\Windows\System32\config\%%H.%%L X:\a04br3\toprgu-audit\%%H.%%L >nul
  if errorlevel 1 goto failed
  X:\a04br3\compare-files.exe C:\Windows\System32\config\%%H.%%L X:\a04br3\toprgu-audit\%%H.%%L
  if errorlevel 1 goto failed
  echo TOPRGU_HIVE_VERIFY=%%H.%%L
  X:\a04br3\audit-hive-uncached.exe %%H.%%L
  if errorlevel 1 goto failed
  echo TOPRGU_HIVE_LOG_COPIED=%%H.%%L
)
copy /b C:\Windows\System32\drivers\mt8167-toprgu.sys X:\a04br3\toprgu-audit\installed.sys >nul
if errorlevel 1 goto failed
X:\a04br3\compare-files.exe X:\a04br3\toprgu-audit\installed.sys X:\a04br3\toprgu-audit-candidate.sys
if errorlevel 1 (echo TOPRGU_SYS_MATCH=NO) else (echo TOPRGU_SYS_MATCH=YES)
reg load HKLM\A04TOPRGUSYS X:\a04br3\toprgu-audit\SYSTEM >nul
if errorlevel 1 goto failed
set "A04_CURRENT="
for /f "tokens=1,2,3" %%A in ('reg query HKLM\A04TOPRGUSYS\Select /v Current 2^>nul') do if /i "%%A"=="Current" set "A04_CURRENT=%%C"
set "A04_SET="
if /i "%A04_CURRENT%"=="0x1" set "A04_SET=ControlSet001"
if /i "%A04_CURRENT%"=="0x2" set "A04_SET=ControlSet002"
if /i "%A04_CURRENT%"=="0x3" set "A04_SET=ControlSet003"
if not defined A04_SET goto fail_sys
echo TOPRGU_SELECTED_CONTROLSET=%A04_SET%
reg query HKLM\A04TOPRGUSYS\%A04_SET%\Services\Mt8167Toprgu /v Start
if errorlevel 1 goto fail_sys
reg query HKLM\A04TOPRGUSYS\%A04_SET%\Services\Mt8167Toprgu /v Group
if errorlevel 1 goto fail_sys
reg query HKLM\A04TOPRGUSYS\%A04_SET%\Services\Mt8167Toprgu /v ImagePath
if errorlevel 1 goto fail_sys
reg unload HKLM\A04TOPRGUSYS >nul
if errorlevel 1 goto unloadfail
reg load HKLM\A04TOPRGUSW X:\a04br3\toprgu-audit\SOFTWARE >nul
if errorlevel 1 goto failed
reg query HKLM\A04TOPRGUSW\Microsoft\SystemCertificates\Root\Certificates\%A04_CERT% /v Blob >nul 2>&1
if errorlevel 1 (echo TOPRGU_TEST_CERT_ROOT=NO) else (echo TOPRGU_TEST_CERT_ROOT=YES)
reg query HKLM\A04TOPRGUSW\Microsoft\SystemCertificates\TrustedPublisher\Certificates\%A04_CERT% /v Blob >nul 2>&1
if errorlevel 1 (echo TOPRGU_TEST_CERT_PUBLISHER=NO) else (echo TOPRGU_TEST_CERT_PUBLISHER=YES)
reg unload HKLM\A04TOPRGUSW >nul
if errorlevel 1 goto unloadfail
echo TOPRGU_AUDIT_FINISHED_READ_ONLY
echo TOPRGU_AUDIT_DOES_NOT_PROVE_SIGNATURE_ACCEPTANCE_OR_PNP
exit /b 0
:fail_sys
reg unload HKLM\A04TOPRGUSYS >nul
if errorlevel 1 goto unloadfail
goto failed
:unloadfail
echo TOPRGU_AUDIT_HIVE_UNLOAD_FAILED
exit /b 3
:rejected
echo TOPRGU_AUDIT_VOLUME_REJECTED
exit /b 4
:invalid
echo TOPRGU_AUDIT_PRECONDITION_REJECTED
exit /b 4
:failed
echo TOPRGU_AUDIT_FAILED
exit /b 2
