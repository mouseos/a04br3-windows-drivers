#!/usr/bin/env python3
import subprocess,tempfile,zlib
from pathlib import Path
script=Path(__file__).resolve().parents[1]/'tools/uart/decode-windows-screen.py'
id='12345678';data=bytes(range(25));crc=lambda d:f'{zlib.crc32(d):08X}'
header=f'A04SC0 {id} 0008 0019 0002\n'
packet=lambda i,d:f'A04SC1 {id} {i:04X} {len(d):02X} {d.hex().upper()} {crc(d)}\n'
a,b=packet(0,data[:24]),packet(1,data[24:]);end=f'A04SCE {id} {crc(data)}\n'
with tempfile.TemporaryDirectory() as t:
 t=Path(t);log=t/'uart';out=t/'out.pbm'
 def run(s):
  log.write_text(s);return subprocess.run(['python3',str(script),'--log',str(log),'--output',str(out)],capture_output=True).returncode
 assert run('noise\n'+header+a+a+b+end)==0
 assert out.read_bytes()==b'P4\n8 25\n'+bytes(x^255 for x in data);out.unlink()
 assert run(header+a+end)!=0 and not out.exists()
 assert run(header+a+b+end.replace(crc(data),'00000000'))!=0 and not out.exists()
 assert run(header+a+b.replace(crc(data[24:]),'00000000')+end)!=0 and not out.exists()
 print('UART screen complete, duplicate, missing and CRC rejection: OK')
