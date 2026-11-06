#!/usr/bin/env python3
"""Run the recovery guard against mocked mount utilities, never a device."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='esp-ro-guard-',dir=root/'build') as td:
 p=Path(td)
 def tool(name,body):
  path=p/name;path.write_text('#!/bin/sh\n'+body+'\n');path.chmod(0o755)
 tool('id','echo 0')
 tool('readlink','case "$*" in *by-name/EFI*) echo /dev/block/mmcblk0p18;; *) echo harmless;; esac')
 tool('cat','case "$*" in */mmcblk0p18/start*) echo 360448;; */mmcblk0p18/size*) echo 524288;; *) /bin/cat "$@";; esac')
 tool('mktemp','exec /usr/bin/mktemp -d "$TMPBASE/mount.XXXXXX"')
 tool('mount','for last; do :; done; [ "$CASE" != mountfail ] || exit 1; case "$CASE" in present|complete|invalid) mkdir "$last/A04BACKUP";; esac; if [ "$CASE" = complete ] || [ "$CASE" = invalid ]; then printf READY > "$last/A04BACKUP/READY"; d=$(/usr/bin/sha256sum "$last/A04BACKUP/READY"); d=${d%% *}; [ "$CASE" != invalid ] || d=bad; echo "A04COMMIT_V1 $d" > "$last/A04BACKUP/COMMIT"; fi; exit 0')
 tool('umount','[ "$CASE" != umountfail ] || exit 1; [ ! -d "$1/A04BACKUP" ] || { rm -f "$1/A04BACKUP/READY" "$1/A04BACKUP/COMMIT"; rmdir "$1/A04BACKUP"; }; exit 0')
 env=dict(os.environ,PATH=str(p)+os.pathsep+os.environ['PATH'],TMPBASE=str(p))
 script=root/'tools/recovery/check-esp-journal.sh'
 for case,rc,marker in [('absent',0,'A04_ESP_JOURNAL_ABSENT'),('present',4,'A04_ESP_JOURNAL_PRESENT'),('complete',0,'A04_ESP_JOURNAL_COMPLETE'),('invalid',4,'A04_ESP_JOURNAL_PRESENT'),('mountfail',5,'A04_ESP_JOURNAL_UNKNOWN'),('umountfail',5,'A04_ESP_JOURNAL_UNKNOWN')]:
  result=subprocess.run(['sh',str(script)],env=dict(env,CASE=case),capture_output=True,text=True)
  assert result.returncode==rc and result.stdout.strip()==marker,(case,result.returncode,result.stdout,result.stderr)
print('ESP read-only mount guard: absent, present, mount and unmount failure passed')
