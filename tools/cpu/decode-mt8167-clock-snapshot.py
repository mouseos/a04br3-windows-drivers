#!/usr/bin/env python3
"""Decode a captured data ABI; never access a device or substitute stock Hz."""
import argparse
import json
import struct
from pathlib import Path

NAMES = ['infra_cpu_mux', 'pll_hp_con0', 'armpll_con0', 'armpll_con1',
         'armpll_pwr', 'mainpll_con0', 'mainpll_con1', 'mainpll_pwr',
         'univpll_con0', 'univpll_con1', 'univpll_pwr']


def decode(data, reference_hz=None, reference_basis=None):
    if len(data) != 128:
        raise ValueError('snapshot must be exactly128 bytes')
    v, size, flags, a, b, count, before, after, *raw = struct.unpack('<6I2Q22I', data)
    if v != 1 or size != 128 or count != 11 or flags & ~127 or (a | b) & ~2047:
        raise ValueError('unknown ABI, flags or valid mask')
    if (flags & 56) != 56:
        raise ValueError('unknown-reference/DDR/effective-CPU markers required')
    if (reference_hz is None) != (reference_basis is None):
        raise ValueError('reference Hz and provenance must be supplied together')
    if reference_hz is not None and not 0 < reference_hz <= 0xffffffff:
        raise ValueError('reference outside uint32 range')
    if reference_basis is not None and not reference_basis.strip():
        raise ValueError('reference provenance required')
    first, second = raw[:11], raw[11:]
    complete = a == b == 2047 and not flags & (2 | 4 | 64) and after >= before
    equal = complete and first == second and not flags & 1
    reasons = []
    rates = dict.fromkeys(['armpll', 'mainpll', 'univpll'])
    if not complete:
        reasons.append('partial/error/admission-lost/nonmonotonic snapshot')
    elif not equal:
        reasons.append('two sweeps differ; no configured rate published')
    elif 0xffffffff in first:
        reasons.append('all-ones register value rejected')
    elif reference_hz is None:
        reasons.append('reference frequency not supplied; raw data only')
    else:
        # Linux MT8167: INTEGER_BITS7, PCW bits21/21/7, postdiv1<<bits26:24.
        # Preserve its ceiling at VCO and postdivider stages; UNIV analogdiv2.
        for name, slot, pcwbits, analogdiv in [('armpll', 2, 21, 1),
                                              ('mainpll', 5, 21, 1),
                                              ('univpll', 8, 7, 2)]:
            con0, con1, power = first[slot:slot+3]
            pcw = con1 & ((1 << pcwbits) - 1)
            if not con0 & 1 or power & 3 != 1 or not pcw:
                reasons.append(name + ': disabled/isolated/zero PCW')
                continue
            if name == 'armpll' and first[1] & 1:
                reasons.append('ARMPLL hopping enabled; PCW is not a qualified rate')
                continue
            frac = 1 << (pcwbits - 7)
            postdiv = 1 << ((con1 >> 24) & 7)
            vco = (reference_hz * pcw + frac - 1) // frac
            rates[name] = ((vco + postdiv - 1) // postdiv) // analogdiv
    parent = ['clk26m_ck', 'armpll', 'univpll', 'mainpll_d2'][(first[0] >> 2) & 3] if a & 1 else None
    return {'schema': 'MT8167_CLOCK_SNAPSHOT_DECODE_V1', 'flags': flags,
            'time_bracket_100ns': [before, after], 'valid_masks': [a, b],
            'first': dict(zip(NAMES, first)), 'second': dict(zip(NAMES, second)),
            'complete': complete, 'equal_observations': equal,
            'atomic_snapshot_proven': False, 'firmware_ABA_excluded': False,
            'CPU_parent_at_first_sample': parent,
            'supplied_reference_hz': reference_hz, 'supplied_reference_basis': reference_basis,
            'configured_PLL_hz_from_supplied_reference': rates,
            'effective_CPU_hz': None, 'DDR_hz': None,
            'physical_voltage_measured': False, 'reasons': reasons,
            'interpretation': 'PLL arithmetic is configuration evidence only; reference accuracy, CPU downstream divider and effective execution rate are not measured.'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('snapshot', type=Path)
    p.add_argument('--reference-hz', type=int)
    p.add_argument('--reference-basis')
    a = p.parse_args()
    try:
        print(json.dumps(decode(a.snapshot.read_bytes(), a.reference_hz, a.reference_basis), indent=2))
    except ValueError as e:
        p.error(str(e))
