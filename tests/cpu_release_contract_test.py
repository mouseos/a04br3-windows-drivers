#!/usr/bin/env python3
"""Structural fail-closed checks on the shipped batch control flow."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'tools/winpe'
a=(root/'audit-toprgu-installed.cmd').read_text().lower()
s=(root/'startnet-cpu-audit.cmd.in').read_text().lower()
r=(root/'release-toprgu-audit-volume.cmd').read_text().lower()
assert a.index('guard-windows-volume.exe\nif errorlevel 1 goto rejected') < a.index('copy /b c:\\windows')
assert 'for %%h in (system software) do for %%l in (log log1 log2)' in a
assert a.index('toprgu_hive_log_copied=') < a.index('reg load hklm\\a04toprgusys')
assert a.count('audit-hive-uncached.exe')==4
assert a.index('audit-hive-uncached.exe system') < a.index('reg load hklm\\a04toprgusys')
assert 'if errorlevel 1 goto failed' in a[a.index('for %%h in (system software)'):a.index('toprgu_hive_log_copied=')]
assert ':rejected\necho toprgu_audit_volume_rejected\nexit /b 4' in a
assert all(f'if "%a04_audit_rc%"=="{n}" goto release' in s for n in (0,2)) and 'if not "%a04_audit_rc%"=="3" goto transmit' in s
assert r.index('guard-windows-volume.exe\nif errorlevel 1 goto rejected') < r.index('reg query hklm >') < r.index('mountvol c: /p')
assert r.count('if not errorlevel 1 goto hiveloaded')==2 and 'if errorlevel 2 goto registryfail' in r
assert 'echo toprgu_volume_released' in r and 'echo mountvol_exit=' in r
assert 'dism.exe' not in s+r+a and 'reboot' not in s+r+a
print('Batch source contracts: guarded copies, log presence, failure release and live-hive exclusion verified; Windows execution not tested')
