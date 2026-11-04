#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import ctypes
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
src = root / 'drivers/power/mt8167-thermal/devinfo.c'
with tempfile.TemporaryDirectory() as d:
    path = pathlib.Path(d)
    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-o', str(path/'lib.so'), str(src)], check=True)
    payload = (63).to_bytes(4,'little') + b''.join(n.to_bytes(4,'little') for n in range(63))
    (path/'in.dts').write_text('/dts-v1/; / { chosen { atag,devinfo = [' + ' '.join(f'{b:02x}' for b in payload) + ']; }; };')
    subprocess.run(['dtc','-I','dts','-O','dtb','-o',str(path/'in.dtb'),str(path/'in.dts')],check=True)
    data = (path/'in.dtb').read_bytes()
    f = ctypes.CDLL(str(path/'lib.so')).a04_devinfo_fdt_extract
    f.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint32)]
    out = (ctypes.c_uint32 * 3)(9,9,9)
    buf = ctypes.create_string_buffer(data)
    assert f(buf,len(data),out)==0 and list(out)==[60,61,62]
    for n in range(len(data)):
        out[:] = [9,9,9]
        assert f(buf,n,out)!=0 and list(out)==[9,9,9]
    bad=bytearray(data); bad[8:12]=(0xffffffff).to_bytes(4,'big')
    assert f(ctypes.create_string_buffer(bytes(bad)),len(bad),out)!=0
    print('FDT boot devinfo: real dtc fixture, all truncations, bad offsets: OK')
