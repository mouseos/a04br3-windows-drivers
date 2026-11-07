#!/usr/bin/env python3
"""Compile the actual feature guard with valid and invalid combinations."""
import subprocess
from pathlib import Path

include = Path(__file__).resolve().parents[1] / 'drivers/storage/mt8167-msdc'
base = ['A04_BOOT_PASSIVE_EXPERIMENT', 'A04_DMA_PROBE', 'A04_READONLY_DISK_PROBE']
cases = [([], True), (['A04_BOOT_MULTIBLOCK_EXPERIMENT=1'], False),
         (base + ['A04_BOOT_MULTIBLOCK_EXPERIMENT=1'], True),
         (base + ['A04_BOOT_MULTIBLOCK_EXPERIMENT=1', 'A04_DMA_MULTIBLOCK_EXPERIMENT=1'], False),
         (base + ['A04_BOOT_MULTIBLOCK_EXPERIMENT=1', 'A04_DMA_WRITE_TEST'], False)]
scratch = ['A04_SCRATCH_IO', 'A04_PASSIVE_IO', 'A04_DMA_PROBE', 'A04_DMA_DISK_PROBE',
           'A04_VIRTUAL_BOUNCE_EXPERIMENT', 'A04_SERIAL_QUEUE_EXPERIMENT',
           'A04_ISOLATED_REQUEST_EXPERIMENT']
cases += [(['A04_SCRATCH_MULTIBLOCK_EXPERIMENT=1'], False),
          (scratch + ['A04_SCRATCH_MULTIBLOCK_EXPERIMENT=1'], True),
          (scratch + ['A04_SCRATCH_MULTIBLOCK_EXPERIMENT=1', 'A04_BOOT_PASSIVE_EXPERIMENT'], False)]
write_flag = ['A04_DMA_WRITE_MULTIBLOCK_EXPERIMENT=1']
cases += [(write_flag, False),
          (base + ['A04_BOOT_MULTIBLOCK_EXPERIMENT=1'] + write_flag, False),
          (base + ['A04_BOOT_MULTIBLOCK_EXPERIMENT=1', 'A04_ENABLE_WRITES'] + write_flag, True),
          (scratch + ['A04_SCRATCH_MULTIBLOCK_EXPERIMENT=1', 'A04_ENABLE_WRITES'] + write_flag, True)]
for defines, expected in cases:
    result = subprocess.run(['cc', '-x', 'c', '-fsyntax-only', '-I', str(include),
                             *['-D' + d for d in defines], '-'],
                            input='#include "multiblock_config.h"\n', text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert (result.returncode == 0) == expected, (defines, result.stderr)
print('multiblock config guards: ok')
