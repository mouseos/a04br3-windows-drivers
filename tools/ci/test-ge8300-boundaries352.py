#!/usr/bin/env python3
"""Compare fixed offline V5 metadata; no GPU or kernel execution."""
from pathlib import Path
import subprocess,argparse,json,hashlib
r=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
fixture=r/'tests/fixtures/ge8300-boundaries352';j=json.loads((fixture/'source.json').read_text())
for n,h in j['files'].items():assert hashlib.sha256((fixture/n).read_bytes()).hexdigest()==h
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(r/'drivers/gpu/mt8167/vendor_m1_11'),str(r/'tests/ge8300_boundaries352_host.c'),'-o',str(o/'check')],check=True)
f=subprocess.run([str(o/'check'),str(fixture/'request-v5.bin'),str(fixture/'blob.bin')],capture_output=True,text=True);(o/'reference-records.txt').write_text(f.stdout);(o/'stderr.txt').write_text(f.stderr);raise SystemExit(f.returncode)
