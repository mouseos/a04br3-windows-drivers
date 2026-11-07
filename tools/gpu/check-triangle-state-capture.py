#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""固定captureとsource CPU builderの全量比較。GPUへ送らずraw blobもsourceへ保存しない。"""
import argparse,ctypes,hashlib,json,struct
from pathlib import Path
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--capture",type=Path,required=True)
    ap.add_argument("--library",type=Path,required=True)
    a=ap.parse_args()
    raw=(a.capture/"runtime-capture-01.bin").read_bytes()
    data=json.loads((a.capture/"parsed-capture.json").read_text())
    assert hashlib.sha256(raw).hexdigest()==data["capture_sha256"]
    lib=ctypes.CDLL(str(a.library.resolve()))
    class PPP(ctypes.Structure):
        _fields_=[(x,ctypes.c_uint32) for x in ["fragment","code","varying","uniform","texture"]]+[
            ("scissor",ctypes.c_uint16),("textured",ctypes.c_uint8)]
    class PDS(ctypes.Structure):
        _fields_=[("data",ctypes.c_ubyte*32),("code",ctypes.c_ubyte*12)]
    lib.a04_m111_build_triangle_prefix.argtypes=[ctypes.c_void_p,ctypes.c_size_t]
    lib.a04_m111_build_triangle_ppp.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(PPP)]
    lib.a04_m111_build_triangle_vertex_pds.argtypes=[ctypes.POINTER(PDS),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint32,ctypes.c_uint32]
    def get(seq,va,n):
        hits=[]
        for e in data["events"]:
            if e["phase"]=="resource" and e["seq"]==seq and e["bytes"]:
                offset=va-int(e["gpu_va"],16)
                if 0<=offset and offset+n<=e["bytes"]:
                    start=e["capture_data_offset"]+offset
                    hits.append(raw[start:start+n])
        assert len(hits)==1,(seq,hex(va),n,len(hits))
        return hits[0]
    results=[]
    for seq in [3,4]:
        ta=bytes.fromhex(next(e["payloads"]["ta"]["hex"] for e in data["events"] if e["phase"]=="pre" and e["seq"]==seq))
        vdm=struct.unpack("<21I",get(seq,struct.unpack_from("<Q",ta,16)[0],84))
        def state(at):
            va=((vdm[at]&255)<<32)|vdm[at+1];n=((vdm[at]>>8)&255)*4
            return get(seq,va,n)
        prefix=ctypes.create_string_buffer(48)
        assert lib.a04_m111_build_triangle_prefix(prefix,48)==0
        assert prefix.raw==state(0)
        expected=state(14);w=struct.unpack("<%dI"%(len(expected)//4),expected)
        p=PPP(w[5],w[6],w[9],w[11 if seq==4 else 10],w[10] if seq==4 else 0,w[4],seq==4)
        out=ctypes.create_string_buffer(len(expected))
        assert lib.a04_m111_build_triangle_ppp(out,len(expected),ctypes.byref(p))==0
        assert out.raw==expected,[(i,hex(x),hex(y)) for i,(x,y) in enumerate(zip(struct.unpack("<%dI"%(len(expected)//4),out.raw),w)) if x!=y]
        p.fragment+=16
        assert lib.a04_m111_build_triangle_ppp(out,len(expected),ctypes.byref(p))==0
        assert struct.unpack_from("<I",out.raw,20)[0]==w[5]+16
        for base,dma_components in [(2,3),(9,2)]:
            dat=get(seq,0xa800000000+vdm[base+1],32)
            usc,dma=struct.unpack_from("<Q",dat)[0]+0xba00000000,struct.unpack_from("<Q",dat,16)[0]
            made=PDS()
            assert lib.a04_m111_build_triangle_vertex_pds(ctypes.byref(made),usc,dma,dma_components,dma_components*4)==0
            assert bytes(made.data)==dat
            assert bytes(made.code)==get(seq,0xa800000000+vdm[base+2],12)
            changed=PDS()
            assert lib.a04_m111_build_triangle_vertex_pds(ctypes.byref(changed),usc+0x100,dma+0x1000,dma_components,dma_components*4)==0
            assert struct.unpack_from("<Q",bytes(changed.data),16)[0]==dma+0x1000
            assert bytes(changed.code)==bytes(made.code)
        results.append({"seq":seq,"prefix_equal_bytes":48,"draw_equal_bytes":len(expected),
                        "vertex_pds_equal_bytes":2*(32+12),"relocation_fields_checked":True})
    return {"capture_sha256":data["capture_sha256"],"comparisons":results,"device_executed":False}
if __name__=="__main__":print(json.dumps(main(),indent=2))
