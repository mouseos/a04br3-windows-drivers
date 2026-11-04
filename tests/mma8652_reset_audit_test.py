#!/usr/bin/env python3
import importlib.util
from pathlib import Path
p = Path(__file__).resolve().parents[1]/'tools/recovery/audit-mma8652-reset-sources.py'
s = importlib.util.spec_from_file_location('audit', p); m = importlib.util.module_from_spec(s); s.loader.exec_module(m)
assert m.classify_unit({'LoadState':'not-found'}) == 'not-found'
assert m.classify_unit({'LoadState':'loaded','ActiveState':'inactive','UnitFileState':'enabled'}) == 'can-be-started-or-unknown'
assert m.classify_unit({'LoadState':'loaded','ActiveState':'active','UnitFileState':'masked'}) == 'hazard-or-unknown'
assert m.classify_unit({'LoadState':'loaded','ActiveState':'inactive','UnitFileState':'masked-runtime'}) == 'inactive-and-masked'
assert m.classify_unit({}) == 'hazard-or-unknown'
print('known reset source classification passed; no device access')
