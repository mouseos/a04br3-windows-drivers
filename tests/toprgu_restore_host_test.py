#!/usr/bin/env python3
"""Ensure missing recovery proof never reaches push or write, even with adb exit 0."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
(root/'build').mkdir(exist_ok=True)
def digest(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
with tempfile.TemporaryDirectory(prefix='restore-test-',dir=root/'build') as d:
 d=Path(d); boot=d/'boot'; boot.write_bytes(b'ANDROID!'+bytes(64)); esp=d/'esp'
 with esp.open('wb') as f:f.truncate(268435456)
 log=d/'calls'; adb=d/'adb'
 adb.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$ADB_TEST_CALLS"\ncase "$*" in *get-state*) echo recovery;; *"shell id -u"*) echo 0;; esac\nexit 0\n');adb.chmod(0o755)
 env=dict(os.environ,PATH=str(d)+os.pathsep+os.environ['PATH'],ADB_TEST_CALLS=str(log))
 script=root/'tools/recovery/restore-toprgu-diagnostic.sh'
 args=['SERIAL',str(boot),digest(boot),str(esp),digest(esp),str(d/'journal')]
 assert subprocess.run(["bash",str(script),'--verify-only',*args],env=env,stdout=subprocess.DEVNULL).returncode==0
 assert not log.exists()
 bad=list(args); bad[2]='0'*64; bad[5]=str(d/'other')
 assert subprocess.run(["bash",str(script),'--execute',*bad],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
 assert not log.exists()
print('restore: verify-only and invalid inputs do not access devices')
