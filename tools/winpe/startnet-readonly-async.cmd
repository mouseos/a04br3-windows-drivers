@echo off
title TAB-A04BR3 async storage diagnostic
wpeinit
echo Starting driver in background...
start "" /b cmd /c "drvload X:\a04br3\a04-msdc-probe.inf > X:\a04br3\drvload.log 2>&1"
ping -n 12 127.0.0.1 >nul
echo === Driver and PnP state ===
reg query HKLM\SYSTEM\CurrentControlSet\Services\a04msdcprobe /v DriverEntrySeen
reg query HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\MOU0001\0 /v Problem
reg query HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\MOU0001\0 /v ConfigFlags
echo === drvload output ===
type X:\a04br3\drvload.log
echo === Disk enumeration ===
(echo list disk&echo list volume&echo exit)>X:\a04br3\list-disks.txt
diskpart /s X:\a04br3\list-disks.txt
echo === Diagnostic finished ===
