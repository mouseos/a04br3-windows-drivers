#!/usr/bin/env python3
"""Mock recovery transport; never connects to a device."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='esp-guard-',dir=root/'build') as td:
 d=Path(td);esp=d/'esp.img';boot=d/'boot.img'
 with esp.open('wb') as f:f.truncate(268435456)
 boot.write_bytes(b'ANDROID!'+bytes(64))
 def sha(p):
  h=hashlib.sha256()
  with p.open('rb') as f:
   for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
  return h.hexdigest()
 digest=sha(esp);manifest=d/'manifest.json';manifest.write_text(json.dumps({'size':268435456,'image_sha256':digest}))
 calls=d/'calls';count=d/'count'
 adb=d/'adb';adb.write_text('''#!/bin/sh
printf '%s\\n' "$*" >> "$CALLS"
case "$*" in
 *'shell sh -s'*) n=0; [ ! -f "$COUNT" ] || n=$(cat "$COUNT"); n=$((n+1)); echo "$n" > "$COUNT"; if [ "$GUARD" = absent ] || { [ "$GUARD" = second ] && [ "$n" = 1 ]; }; then echo A04_ESP_JOURNAL_ABSENT; exit 0; fi; if [ "$GUARD" = present ]; then echo A04_ESP_JOURNAL_PRESENT; exit 4; fi; echo A04_ESP_JOURNAL_UNKNOWN; exit 5;;
 *A04_RESTORE_GEOMETRY_OK*) echo A04_RESTORE_GEOMETRY_OK;; *A04_RESTORE_STAGING_OK*) echo A04_RESTORE_STAGING_OK;;
 *A04_RESTORE_BOOT_SYNCED*) echo A04_RESTORE_BOOT_SYNCED;; *A04_RESTORE_ESP_SYNCED*) echo A04_RESTORE_ESP_SYNCED;;
 *get-state*) echo recovery;; *'shell id -u'*) echo 0;; *'shell readlink'*) echo /dev/block/mmcblk0p18;;
 *'/mmcblk0p18/start'*) echo 360448;; *'/mmcblk0p18/size'*) echo 524288;; *'/mmcblk0p18/dev'*) echo 179:18;;
 *'/proc/self/mountinfo'*) :;; *'shell sha256sum'*) echo "$IMG_HASH  /tmp/staged";;
 *'shell dd'*) echo '268435456 bytes copied';;
esac
exit 0
''');adb.chmod(0o755)
 fastboot=d/'fastboot';fastboot.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$CALLS"\ncase "$*" in *devices*) printf "SERIAL\\tfastboot\\n";; esac\nexit 0\n');fastboot.chmod(0o755)
 env=dict(os.environ,PATH=str(d)+os.pathsep+os.environ['PATH'],CALLS=str(calls),COUNT=str(count),IMG_HASH=digest,A04_SERIAL='SERIAL')
 scripts={
  'python':['python3',str(root/'tools/recovery/flash-esp.py'),'--image',str(esp),'--manifest',str(manifest),'--flash-log',str(d/'flash.log')],
  'shell':['bash',str(root/'tools/recovery/flash-efi.sh'),str(esp)],
  'restore':['bash',str(root/'tools/recovery/restore-toprgu-diagnostic.sh'),'--execute','SERIAL',str(boot),sha(boot),str(esp),digest,str(d/'restore-log')],
 }
 for name,cmd in scripts.items():
  for case in ('present','unknown','second','absent'):
   for f in (calls,count,d/'flash.log'):
    if f.exists():f.unlink()
   log=d/'restore-log'
   if log.exists():
    import shutil;shutil.rmtree(log)
   result=subprocess.run(cmd,env=dict(env,GUARD=case),capture_output=True,text=True)
   text=calls.read_text() if calls.exists() else ''
   writes=('shell dd' in text or 'A04_RESTORE_BOOT_SYNCED' in text or 'dd if=' in text)
   if case=='absent':assert result.returncode==0 and writes,(name,case,result.stdout,result.stderr,text)
   else:assert result.returncode!=0 and not writes,(name,case,result.stdout,result.stderr,text)
print('ESP journal guard: absent permits write; present/unknown/changed blocks writes')
