@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Diagnostic prototype: all output is in RAM; never auto-reset.
set "A04_REPORT=X:\a04br3\toprgu-audit-result.txt"
> "%A04_REPORT%" echo AUDIT_BEGIN
X:\a04br3\map-offline-windows.exe >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto mapfail
X:\a04br3\guard-windows-volume.exe >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto guardfail
X:\a04br3\audit-volume-sys.exe >> "%A04_REPORT%" 2>&1
set "A04_VOLUME_RC=%errorlevel%"
>> "%A04_REPORT%" echo AUDIT_VOLUME_EXIT=%A04_VOLUME_RC%
X:\a04br3\audit-copy-hives.exe >> "%A04_REPORT%" 2>&1
set "A04_COPY_RC=%errorlevel%"
>> "%A04_REPORT%" echo AUDIT_HIVE_COPY_EXIT=%A04_COPY_RC%
if not "%A04_COPY_RC%"=="0" goto copyfail
reg load HKLM\A04AUDITSYS X:\a04br3\audit-system\SYSTEM >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto loadfail
X:\a04br3\audit-pnp-inf.exe >> "%A04_REPORT%" 2>&1
set "A04_PNP_RC=%errorlevel%"
>> "%A04_REPORT%" echo AUDIT_PNP_EXIT=%A04_PNP_RC%
reg unload HKLM\A04AUDITSYS >> "%A04_REPORT%" 2>&1
if errorlevel 1 goto unloadfail
if not "%A04_PNP_RC%"=="0" goto pnpfail
if not "%A04_VOLUME_RC%"=="0" goto volumefail
>> "%A04_REPORT%" echo AUDIT_STATUS=PNP_EVIDENCE_REQUIRES_REVIEW
set "A04_AUDIT_RC=10"
goto release
:copyfail
>> "%A04_REPORT%" echo AUDIT_STATUS=HIVE_COPY_REJECTED
set "A04_AUDIT_RC=3"
goto release
:loadfail
>> "%A04_REPORT%" echo AUDIT_STATUS=RAM_HIVE_LOAD_FAILED
set "A04_AUDIT_RC=4"
goto release
:unloadfail
>> "%A04_REPORT%" echo AUDIT_STATUS=RAM_HIVE_UNLOAD_FAILED
set "A04_AUDIT_RC=5"
goto release
:pnpfail
>> "%A04_REPORT%" echo AUDIT_STATUS=PNP_FAILED
set "A04_AUDIT_RC=6"
goto release
:volumefail
>> "%A04_REPORT%" echo AUDIT_STATUS=VOLUME_OR_SYS_PARTIAL
set "A04_AUDIT_RC=7"
:release
X:\a04br3\release-offline-volume.exe >> "%A04_REPORT%" 2>&1
set "A04_RELEASE_RC=%errorlevel%"
>> "%A04_REPORT%" echo AUDIT_RELEASE_EXIT=%A04_RELEASE_RC%
goto transmit
:mapfail
>> "%A04_REPORT%" echo AUDIT_STATUS=VOLUME_MAPPING_REJECTED
set "A04_AUDIT_RC=2"
set "A04_RELEASE_RC=NOT_ATTEMPTED"
goto transmit
:guardfail
>> "%A04_REPORT%" echo AUDIT_STATUS=VOLUME_IDENTITY_REJECTED_NO_RELEASE
set "A04_AUDIT_RC=2"
set "A04_RELEASE_RC=NOT_ATTEMPTED"
:transmit
>> "%A04_REPORT%" echo AUDIT_EXIT=%A04_AUDIT_RC%
X:\a04br3\uart-audit-stream.exe 981D5A3EB11CA480
set "A04_UART_RC=%errorlevel%"
echo AUDIT_UART_QUEUE_EXIT=%A04_UART_RC%
if not "%A04_UART_RC%"=="0" exit /b 8
if not "%A04_RELEASE_RC%"=="0" exit /b 9
exit /b %A04_AUDIT_RC%
