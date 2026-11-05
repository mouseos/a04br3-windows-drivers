#!/usr/bin/env python3
"""Compare two captured WinPE reports with an independently saved 16 MiB reference.

This program reads files only.  It cannot establish target identity, reference
provenance, capture completeness, or that the medium was unchanged between reads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re

WORKSPACE = Path(__file__).resolve().parents[4]
SIZE = 4096 * 4096


def workspace_file(value):
    path = Path(value).resolve(strict=True)
    if not path.is_file() or not path.is_relative_to(WORKSPACE):
        raise ValueError('input must be a file within the workspace')
    return path


def digest_reference(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        before = os.fstat(f.fileno())
        total = 0
        while True:
            b = f.read(1024 * 1024)
            if not b:
                break
            total += len(b)
            if total > SIZE:
                raise ValueError('reference must be exactly 16 MiB')
            h.update(b)
        after = os.fstat(f.fileno())
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
        raise ValueError('reference changed during reading')
    if total != SIZE:
        raise ValueError('reference must be exactly 16 MiB')
    return h.hexdigest()


def parse_report(path):
    raw = path.read_bytes()
    if len(raw) > 1048576:
        raise ValueError('report too large')
    lines = raw.decode('ascii', errors='strict').splitlines()
    def one(pattern):
        found = [(i, m) for i, line in enumerate(lines) if (m := re.fullmatch(pattern, line))]
        if len(found) != 1:
            raise ValueError('missing, malformed or repeated report field')
        return found[0]
    start_i, _ = one(r'MSDC_READ_STRESS_STARTED')
    pass_i, _ = one(r'MSDC_READ_STRESS_PASSED')
    measure_i, measure = one(r'MSDC_READ_STRESS_PAIRS=([0-9]+) ELAPSED_MS=([0-9]+)')
    checksum_i, checksum_match = one(r'MSDC_REFERENCE_SHA256=([0-9a-f]{64})')
    if not start_i < measure_i < checksum_i < pass_i:
        raise ValueError('report fields out of order')
    checksum = checksum_match.group(1)
    pairs, elapsed = map(int, measure.groups())
    if not 128 <= pairs <= 16384 or not 0 < elapsed <= 120000:
        raise ValueError('invalid measurement')
    if any('MSDC_READ_STRESS_FAILED' in line for line in lines):
        raise ValueError('report contains failure marker')
    return {'pairs': pairs, 'elapsed_ms': elapsed, 'reference_sha256': checksum,
            'paired_read_bytes': pairs * 8192, 'report_sha256': hashlib.sha256(raw).hexdigest()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference', required=True)
    p.add_argument('--single-report', required=True)
    p.add_argument('--multi-report', required=True)
    a = p.parse_args()
    try:
        reference, single, multi = map(workspace_file, (a.reference, a.single_report, a.multi_report))
        if len({reference, single, multi}) != 3:
            raise ValueError('three distinct inputs are required')
        digest = digest_reference(reference)
        first, second = parse_report(single), parse_report(multi)
        if first['report_sha256'] == second['report_sha256']:
            raise ValueError('single and multi reports are identical')
        if first['reference_sha256'] != digest or second['reference_sha256'] != digest:
            raise ValueError('independent reference digest mismatch')
    except (OSError, UnicodeError, ValueError) as exc:
        p.error(str(exc))
    print(json.dumps({'independent_reference_sha256': digest, 'single': first, 'multi': second,
        'limitations': ['Files do not establish the physical device, read range, run identity, configuration, or reference provenance',
                        'Pairwise consistency and reference match do not prove PnP, DMA, or long-term stability']}, indent=2))

if __name__ == '__main__':
    main()
