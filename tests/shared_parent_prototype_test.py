#!/usr/bin/env python3
"""Cross-check the offline PnP fixture, client IDs and resource separation."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
proto=root/'drivers/bus/mt8167-shared/prototype'
source=(root/'drivers/bus/mt8167-shared/wdf_parent_probe.c').read_text()
asl=(proto/'shared-parent.asl').read_text()
ranges=[(int(a,16),int(b,16)) for a,b in re.findall(r'Memory32Fixed\s*\(ReadWrite,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)',asl)]
assert set(ranges)=={(0x10000000,0x1000),(0x10001000,0x1000),(0x10005000,0x1000),(0x1000f000,0x1000),(0x10018000,0x710)}
assert len(ranges)==5 and 'BEN0001' in asl
assert re.search(r'Name\s*\(\s*_STA\s*,\s*Zero\s*\)',asl)
assert r'ACPI\BEN0001' in (proto/'shared-parent.inf').read_text()
for inf,hwid,base in [('shared-buttons.inf',r'MT6392\Buttons',0x10002000),('shared-touch.inf',r'MT8167\BU21025',0x11009000)]:
    assert hwid in (proto/inf).read_text()
    assert hwid.replace('\\','\\\\') in source and hex(base) in source.lower()
    assert all(not (base<start+size and start<base+0x1000) for start,size in ranges)
assert r'MT8167\USB20' in (root/'drivers/usb/mt8167-usb20/mt8167-usb20.inf').read_text()
assert r'MT8167\\USB20' in source
with tempfile.TemporaryDirectory() as tmp:
    subprocess.run(['iasl','-p',str(Path(tmp)/'parent'),str(proto/'shared-parent.asl')],check=True,stdout=subprocess.DEVNULL)
print('shared parent fixture and child resource separation: OK')
