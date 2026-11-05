#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Create synthetic m1.11 input and expected unpacked buffers for host tests."""
import struct
from pathlib import Path
out=Path('build/vendor-m1-11-test');out.mkdir(parents=True,exist_ok=True)
b=bytearray(8192)
struct.pack_into('<16sHHIIIIIHHHHHH',b,0,b'\x7fELF\x01\x01\x01'+bytes(9),2,8,1,0xc0000001,52,0,0,52,32,4,0,0,0)
sections=[(4,1,0xc0000000,0x2f000,0x17000,0),(5,1,0x9fc02000,0x2000,0x1000,0x17000),(6,1,0xbfc00000,0x1000,0x1000,0x18000),(7,2,0xc0032000,0x8000,0x3000,0),(8,2,0xbfc01000,0x1000,0x1000,0x3000),(9,2,0xcf600000,0x1000,0x1000,0x4000)]
struct.pack_into('<4IQI',b,4096,1,32,6,24,0x001600280036001e,0)
for i,s in enumerate(sections): struct.pack_into('<6I',b,4128+i*24,*s)
code=bytearray(0x19000);data=bytearray(0x5000)
for i,(addr,offset,flags,target) in enumerate([(0xc0000000,0,5,code),(0x9fc02000,0x17000,5,code),(0xbfc00000,0x18000,5,code),(0xc0032000,0,6,data)]):
    content=bytes([0x41+i])*8;pos=256+i*8;b[pos:pos+8]=content
    struct.pack_into('<8I',b,52+i*32,1,pos,addr,0,8,16,flags,4);target[offset:offset+8]=content
(out/'firmware.bin').write_bytes(b);(out/'code.bin').write_bytes(code);(out/'data.bin').write_bytes(data)
