#!/usr/bin/env python3
"""A nonbootable WIM must be rejected before creating or modifying the ESP."""
from pathlib import Path
import os,struct,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='a04-boot-index-',dir=os.environ['TMPDIR']) as name:
    d=Path(name);template=d/'base.img'
    with template.open('wb') as f:f.truncate(256*1024*1024)
    for count,index in ((1,0),(2,1),(0,1),(1,2)):
        data=bytearray(208);data[:8]=b'MSWIM\0\0\0';struct.pack_into('<I',data,8,208)
        struct.pack_into('<I',data,44,count);struct.pack_into('<I',data,120,index)
        wim=d/'candidate.wim';wim.write_bytes(data);out=d/'out.img'
        result=subprocess.run([sys.executable,str(root/'tools/winpe/replace-esp-wim.py'),
            '--template',str(template),'--wim',str(wim),'--output',str(out),
            '--template-bcd-sha256','0'*64,'--template-wim-sha256','0'*64],capture_output=True,text=True)
        assert result.returncode and f'image_count={count}, boot_index={index}' in result.stderr
        assert not out.exists()
print('nonbootable and ambiguous WIMs rejected before ESP creation')
