@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Run from an elevated console in installed Windows, with the status tool.
rem Capture the entire console externally; this script does not reboot or edit C:.
if /i "%SystemDrive%"=="X:" goto winpe
reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT >nul 2>&1
if not errorlevel 1 goto winpe
echo TOPRGU_INSTALLED_DIAGNOSTIC_BEGIN
echo DATE=%date% TIME=%time%
echo NUMBER_OF_PROCESSORS=%NUMBER_OF_PROCESSORS%
echo === SERVICE_CONFIGURATION ===
reg query HKLM\SYSTEM\CurrentControlSet\Services\Mt8167Toprgu /v Start
reg query HKLM\SYSTEM\CurrentControlSet\Services\Mt8167Toprgu /v Group
reg query HKLM\SYSTEM\CurrentControlSet\Services\Mt8167Toprgu /v ImagePath
sc query Mt8167Toprgu
echo === PROCESSOR_REGISTRY ===
reg query HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor
echo === PNP_STATUS ===
if exist "%~dp0toprgu-provider-status.exe" (
  "%~dp0toprgu-provider-status.exe"
) else (
  echo TOPRGU_STATUS_TOOL_MISSING
)
echo === PERFORMANCE_COUNTER_SAMPLES ===
where typeperf >nul 2>&1
if errorlevel 1 goto nocounters
rem Localization can make this counter unavailable. Its absence is not a pass.
typeperf "\Processor(_Total)\%% Processor Time" -si 5 -sc 3
if errorlevel 1 echo TOPRGU_COUNTER_SAMPLE_FAILED
goto done
:nocounters
echo TOPRGU_COUNTER_TOOL_MISSING
:done
echo TOPRGU_INSTALLED_DIAGNOSTIC_END_REVIEW_REQUIRED
echo TOPRGU_CPU_COUNT_IS_NOT_OWNERSHIP_OR_STABILITY_PROOF
exit /b 0
:winpe
echo TOPRGU_INSTALLED_DIAGNOSTIC_REJECTED_WINPE
exit /b 2
