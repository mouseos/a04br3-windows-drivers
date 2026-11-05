#!/usr/bin/env python3
import importlib.util
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('toprgu_uart',root/'tools/recovery/analyze-toprgu-handoff.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
e=b'A04TRACE ExitBootServices event signaled\r\n';o=b'A04RGU CPU1 ownership acquired CPUs>=4\n';k=b'A04RGU timer kick\n'
assert m.analyze(b'BOOT_REASON: 4\n'+e+o+k+k)['uart_handoff_observed']
for x in (o+k+k, e+k+k, e+o+k, e+o+k+k+b'BOOT_REASON: 9\n', o+e+k+k, e+b'BOOT_REASON: 9\n'+o+k+k):
 assert not m.analyze(x)['uart_handoff_observed']
print('UART handoff ordering, reset boundary, incomplete capture: OK')
