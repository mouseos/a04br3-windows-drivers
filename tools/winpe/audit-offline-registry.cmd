@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Only use a SYSTEM hive copied to X: by a separately verified collector.
rem This script never loads the C: hive. It does not resolve selected INF/CAT.
if not exist X:\a04br3\audit-system\SYSTEM goto missing
reg query HKLM\A04AUDITSYS >nul 2>&1
if not errorlevel 1 goto alreadyloaded
reg load HKLM\A04AUDITSYS X:\a04br3\audit-system\SYSTEM >nul 2>&1
if errorlevel 1 goto loadfail
reg query HKLM\A04AUDITSYS\Select /v Current
reg query HKLM\A04AUDITSYS\Select /v Default
rem Preliminary coverage is limited to four named ControlSets; Select is reported.
rem Any other selected set is not audited by this diagnostic.
for %%C in (001 002 003 004) do call :controlset %%C
reg unload HKLM\A04AUDITSYS >nul 2>&1
if errorlevel 1 goto unloadfail
echo A04REG_PRELIMINARY_ONLY_CONTROLSETS_001_004_INF_CAT_UNRESOLVED
exit /b 0
:controlset
echo A04REG_CONTROLSET_%1_BEGIN
for %%S in (Mt8167Toprgu Mt8167Shared Bu21025Shared mshidkmdf) do (
 echo A04REG_SERVICE_%%S
 reg query HKLM\A04AUDITSYS\ControlSet%1\Services\%%S /s
)
for %%D in (ACPI\MOU0004 ACPI\MOU0005 ACPI\BEN0001 MT8167\BU21025) do (
 echo A04REG_ENUM_%%D
 reg query HKLM\A04AUDITSYS\ControlSet%1\Enum\%%D /s
)
echo A04REG_CONTROLSET_%1_END
exit /b 0
:missing
echo A04REG_RAM_HIVE_MISSING
exit /b 2
:alreadyloaded
echo A04REG_HIVE_NAME_BUSY
exit /b 2
:loadfail
echo A04REG_RAM_HIVE_LOAD_FAILED
exit /b 3
:unloadfail
echo A04REG_RAM_HIVE_UNLOAD_FAILED
exit /b 4
