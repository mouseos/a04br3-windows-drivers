import importlib.util, pathlib, struct
p=pathlib.Path(__file__).resolve().parents[2]/'tools/winpe/decode-usb-bqhr.py'
s=importlib.util.spec_from_file_location('bqhr',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
w=[0x314f524147484334,128,2,1,2,3,4,0,255,7,255,9,0,0,0,0]
b=bytearray(struct.pack('<16Q',*w));h=14695981039346656037
for x in b[:120]:h=((h^x)*1099511628211)&0xffffffffffffffff
struct.pack_into('<Q',b,120,h)
lines=['A04BQHR SNAP AABBCCDD 0 00000003 00000000 00000000 00000000']
for i in range(8):lines.append('A04BQHR SNAP AABBCCDD %d %s'%(i+1,' '.join('%08X'%x for x in struct.unpack_from('<4I',b,i*16))))
r=m.decode('\n'.join(lines));assert r[0]['record']['state']=='0x0000000000000002' and r[0]['raw_hex']==b.hex()
for invalid in (lines[:-1],lines+[lines[1][:-1]+('0' if lines[1][-1]!='0' else '1')],lines[:2]+[lines[2][:-1]+('0' if lines[2][-1]!='0' else '1')]+lines[3:]):
 try:m.decode('\n'.join(invalid))
 except ValueError:pass
 else:raise AssertionError('invalid frame accepted')
assert m.decode('A04BQHR SNAP 12345678 0 FFFFFFFF 00000005 00000000 00000000')[0]['transport_result']==0xffffffff
print('BQHR UART decoder validation passed')
