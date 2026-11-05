#!/usr/bin/env python3
"""Decode cached A04BAT owner observations; no hardware or I2C access."""
import argparse
import json
import re
from pathlib import Path

RECORD = re.compile(r'A04BAT OWNER phase=(\w+) life=([0-9A-F]{8}) valid=([0-9A-F]{8}) regs=([0-9A-F]{64}) faults=([0-9A-F]{8}) kicks=([0-9A-F]{8}) io_fault=([0-9A-F]{8}) stopped=([0-9A-F]{8})')


def decode(line):
    m = RECORD.search(line)
    if not m:
        return None
    phase, life, valid, raw, faults, kicks, io_fault, stopped = m.groups()
    valid = int(valid, 16)
    words = [int(raw[i:i + 8], 16) for i in range(0, 64, 8)]
    if valid & ~0xff or any(x > 0xff for x in words):
        raise ValueError('invalid snapshot encoding')
    reg = [v if valid & (1 << i) else None for i, v in enumerate(words)]
    result = {'phase': phase, 'lifecycle': int(life, 16), 'valid_mask': valid,
              'cached_registers': reg, 'fault_history': int(faults, 16),
              'kicks': int(kicks, 16), 'transport_fault': int(io_fault, 16),
              'owner_stopped': bool(int(stopped, 16)),
              'registers_are_pre_reset_cache': phase == 'POST_RESET_CACHED'}
    if reg[0] is not None:
        result.update(status=(reg[0] >> 4) & 7, fault=reg[0] & 7)
    if reg[1] is not None:
        result['battery_status'] = (reg[1] >> 1) & 3
        result['battery_ovp_observed'] = ((reg[1] >> 1) & 3) == 1
    if reg[3] is not None:
        result['programmed_voltage_mv'] = 3500 + 20 * (reg[3] >> 2)
    if reg[5] is not None:
        result['programmed_current_ma'] = 550 + 75 * (reg[5] >> 3)
    if reg[7] is not None:
        result['safety_timer'] = ['27min', '6h', '9h', 'disabled'][(reg[7] >> 5) & 3]
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('uart_log', type=Path)
    args = parser.parse_args()
    log = args.uart_log.read_text(encoding='utf-8', errors='replace')
    gauge = re.findall(r'A04BAT STOPPED GAUGE mv=([0-9A-F]{8}) pct=([0-9A-F]{8})', log)
    errors = re.findall(r'A04BAT STOPPED GAUGE ERROR reason=([0-9A-F]{8})', log)
    result = {'stopped_gauge_only': 'A04BAT STOPPED GAUGE ONLY NO BQ' in log,
              'gauge_samples': [{'voltage_mv': int(mv, 16), 'percent': int(percent, 16)}
                                for mv, percent in gauge],
              'gauge_errors': [int(value, 16) for value in errors],
              'admission_refused': 'A04BAT OWNER ADMISSION REFUSED' in log,
              'default_write_completed': 'A04BAT DEFAULT WRITE OK' in log,
              'default_unconfirmed': 'A04BAT DEFAULT UNCONFIRMED' in log,
              'observations': [x for line in log.splitlines() if (x := decode(line))],
              'limitations': ['RESET bus completion does not prove DEFAULT mode',
                              'POST_RESET_CACHED values precede RESET',
                              'reading BQ after RESET restarts its watchdog',
                              'snapshot does not prove source or charging current']}
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
