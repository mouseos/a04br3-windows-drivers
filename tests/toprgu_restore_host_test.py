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
 assert subprocess.run([str(script),'--verify-only',*args],env=env,stdout=subprocess.DEVNULL).returncode==0
 assert not log.exists()
 assert subprocess.run([str(script),'--execute',*args],env=env,stdout=subprocess.DEVNULL).returncode!=0
 calls=log.read_text(); assert 'get-state' in calls and 'push' not in calls and 'dd if=' not in calls
 bad=list(args); bad[2]='0'*64
 log.unlink(); bad[5]=str(d/'other')
 assert subprocess.run([str(script),'--execute',*bad],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
 assert not log.exists()
 adb.write_text('''#!/bin/sh
printf "%s\\n" "$*" >> "$ADB_TEST_CALLS"
case "$*" in
 *get-state*) echo recovery;; *"shell id -u"*) echo 0;;
 *A04_RESTORE_GEOMETRY_OK*) echo A04_RESTORE_GEOMETRY_OK;;
 *A04_RESTORE_STAGING_OK*) echo A04_RESTORE_STAGING_OK;;
 *A04_RESTORE_BOOT_SYNCED*) echo A04_RESTORE_BOOT_SYNCED;;
 *A04_RESTORE_ESP_SYNCED*) echo A04_RESTORE_ESP_SYNCED;;
esac
exit 0
''')
 good=list(args);good[5]=str(d/'good-journal')
 assert subprocess.run([str(script),'--execute',*good],env=env,stdout=subprocess.DEVNULL).returncode==0
 calls=log.read_text(); assert calls.index('A04_RESTORE_STAGING_OK')<calls.index('A04_RESTORE_BOOT_SYNCED')<calls.index('A04_RESTORE_ESP_SYNCED')
 assert 'reboot' not in calls
print('restore: local verification has no device access; missing marker and wrong hash fail before writes')
