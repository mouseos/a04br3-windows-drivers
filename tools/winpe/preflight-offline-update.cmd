@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Read-only preflight only. The RAM hive copy is not an exclusive snapshot.
X:\a04br3\offline-baseline-guard.exe
if errorlevel 1 goto baselinefail
X:\a04br3\audit-copy-hives.exe
if errorlevel 1 goto copyfail
reg load HKLM\A04AUDITSYS X:\a04br3\audit-system\SYSTEM
if errorlevel 1 goto loadfail
X:\a04br3\offline-pnp-baseline.exe
set "A04_PNP_RC=%errorlevel%"
reg unload HKLM\A04AUDITSYS
if errorlevel 1 goto unloadfail
if not "%A04_PNP_RC%"=="0" goto pnpproblem
X:\a04br3\offline-hive-apply.exe --payload-check
if errorlevel 1 goto payloadfail
X:\a04br3\offline-signature-check.exe
if errorlevel 1 goto signfail
echo UPDATE_PREFLIGHT_PASSED_SNAPSHOT_AND_RT_POLICY_UNPROVEN_NO_UPDATE
exit /b 0
:baselinefail
echo UPDATE_PREFLIGHT_BASELINE_REJECTED
exit /b 2
:copyfail
echo UPDATE_PREFLIGHT_RAM_COPY_REJECTED
exit /b 3
:loadfail
echo UPDATE_PREFLIGHT_RAM_HIVE_LOAD_FAILED
exit /b 4
:unloadfail
echo UPDATE_PREFLIGHT_RAM_HIVE_UNLOAD_FAILED
exit /b 5
:pnpproblem
echo UPDATE_PREFLIGHT_PNP_REJECTED
exit /b 6
:payloadfail
echo UPDATE_PREFLIGHT_PAYLOAD_REJECTED
exit /b 7
:signfail
echo UPDATE_PREFLIGHT_WINPE_SIGNATURE_REJECTED
exit /b 8
