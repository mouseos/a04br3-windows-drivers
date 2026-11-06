#!/usr/bin/env python3
"""Check INF display-name validation without rewriting signed package files."""
import importlib.util
from pathlib import Path
import tempfile
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('names',root/'tools/winpe/verify-hardware-display-names.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='inf-name-test-',dir=root.parents[1]/'.codex-work') as tmp:
    p=Path(tmp)/'test.inf'
    p.write_text('[Version]\nSignature="$WINDOWS NT$"\n[CopyFiles]\ndriver.sys\n[Strings]\nDevice="ROHM BU21025 Touchscreen"\n')
    assert m.check(p,'device','ROHM BU21025 Touchscreen')=='ROHM BU21025 Touchscreen'
    for name in ('A04 Touchscreen','Experimental BU21025','ROHM Wrong Chip'):
        p.write_text('[Strings]\nDevice="'+name+'"\n')
        try:m.check(p,'device','ROHM BU21025 Touchscreen')
        except ValueError:pass
        else:raise AssertionError('incorrect display name accepted')
    p.write_text('[Strings]\nDevice="ROHM BU21025 Touchscreen"\n[Strings]\nDevice="ROHM BU21025 Touchscreen"\n')
    try:m.check(p,'device','ROHM BU21025 Touchscreen')
    except ValueError:pass
    else:raise AssertionError('duplicate Strings section accepted')
print('HARDWARE_DISPLAY_NAME_CHECK_PASS')
