#!/usr/bin/env python3
import importlib.util
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'tools/uart/analyze_bq_owner.py'
spec = importlib.util.spec_from_file_location('bq_owner_uart', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def record(mask, values, phase='POST_RESET_CACHED'):
    words = ''.join(f'{x:08X}' for x in values)
    return (f'A04BAT OWNER phase={phase} life=00000004 valid={mask:08X} regs={words} '
            'faults=00000001 kicks=00000002 io_fault=00000000 stopped=00000001')

sample = module.decode(record(255, [0x30, 0, 0x8c, 0xa4, 0x45, 0x51, 0, 0x28]))
assert sample['programmed_voltage_mv'] == 4320
assert sample['programmed_current_ma'] == 1300
assert sample['safety_timer'] == '6h'
assert sample['registers_are_pre_reset_cache'] and not sample['battery_ovp_observed']
partial = module.decode(record(0x03, [7, 2, 0, 0, 0, 0, 0, 0], 'POLL_ERROR'))
assert partial['battery_ovp_observed'] and 'programmed_voltage_mv' not in partial
assert module.decode('unrelated') is None
try:
    module.decode(record(0x100, [0] * 8))
except ValueError:
    pass
else:
    raise AssertionError('invalid mask accepted')
print('BQ cached UART snapshot decoding: PASS')
