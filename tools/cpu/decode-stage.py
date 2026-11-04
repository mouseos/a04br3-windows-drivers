#!/usr/bin/env python3
"""Decode saved A04BR3 CPU stage snapshots without accessing hardware."""
import argparse
import json
import re

FIELDS = ('mux', 'pll0', 'pll1')
PATTERN = re.compile(r'\b(mux|pll0|pll1)=([0-9a-fA-F]{8})(?![0-9a-fA-F])')


def decode(line):
    pairs = PATTERN.findall(line)
    if any(sum(key == field for key, _ in pairs) != 1 for field in FIELDS):
        raise ValueError('each of mux, pll0, and pll1 must occur exactly once')
    regs = {key: int(value, 16) for key, value in pairs}
    result = {'mux': regs['mux'], 'pll0': regs['pll0'], 'pll1': regs['pll1'],
              'configured_pll_hz': None, 'effective_cpu_hz': None,
              'interpretation': 'unknown'}
    # Decode only the exact proven clock selection and divider used by the
    # existing board-specific SEC path; other states are deliberately unknown.
    pcw = regs['pll1'] & 0x1fffff
    divider = (regs['pll1'] >> 24) & 7
    if ((regs['mux'] >> 2) & 3) == 1 and (regs['pll0'] & 1) and divider == 0 and pcw:
        result['configured_pll_hz'] = 26_000_000 * pcw // (1 << 14)
        result['interpretation'] = 'configured PLL only; CPU frequency not measured'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', help='JSON object with SEC/EBS snapshot strings')
    args = parser.parse_args()
    try:
        with open(args.snapshot, encoding='utf-8') as stream:
            data = json.load(stream)
        result = {stage: decode(data[stage]) for stage in ('SEC', 'EBS')}
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f'invalid snapshot: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
