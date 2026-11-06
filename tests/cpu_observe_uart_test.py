import importlib.util
from pathlib import Path
import zlib
path=Path(__file__).resolve().parents[1]/'tools/cpu/decode-cpu-observe.py'
spec=importlib.util.spec_from_file_location('obs',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def frame(vals):
    s=b'A04CP1 '+b' '.join(f'{v:08X}'.encode() for v in vals)
    return s+b' '+f'{zlib.crc32(s):08X}'.encode()+b'\n'
a=frame([1,0,0,3,1300000000,1300000,0]);b=frame([1,0,1,4,1,0xc8000,1]);bad=a[:-3]+b'00\n'
r=module.decode(a+a+b+bad);assert r['invalid_crc']==1
assert r['runs']['00000001']['0']['00000000']==[3,1300000000,1300000,0]
assert 'contradictory' not in r['runs']['00000001']['0']
c=frame([1,0,0,3,1001000000,1300000,0]);assert module.decode(a+c)['runs']['00000001']['0']['contradictory']
print('CPU observation UART CRC, duplicate and conflict checks passed')
