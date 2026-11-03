#!/usr/bin/env python3
"""実験BCDにシステムストア識別値を一つ追加する。"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if not a.input.is_file() or a.output.exists():
    raise SystemExit('入力がないか出力が既にあります')
prefix = r'HKEY_LOCAL_MACHINE\BCD00000000'
with tempfile.TemporaryDirectory(prefix='a04-bcd-') as d:
    d = Path(d)
    original, candidate = d / 'original', d / 'candidate'
    old_export, new_export = d / 'old.reg', d / 'new.reg'
    shutil.copyfile(a.input, original)
    shutil.copyfile(original, candidate)
    def export(hive, output):
        subprocess.run(['reged', '-x', str(hive), prefix, '\\', str(output)], check=True,
                       stdout=subprocess.DEVNULL)
        return output.read_text(encoding='utf-8')
    old = export(original, old_export)
    header = f'[{prefix}\\Description]\n'
    if old.count(header) != 1 or '"System"=' in old or '"TreatAsSystem"=' in old:
        raise SystemExit('元BCDのDescriptionが想定外です')
    patch = d / 'patch.reg'
    patch.write_text('Windows Registry Editor Version 5.00\n\n' + header +
                     '"System"=dword:00000001\n', encoding='ascii')
    # chntpw reged returns 2 after successfully writing a changed hive.
    result = subprocess.run(['reged', '-C', '-I', str(candidate), prefix, str(patch)],
                            stdout=subprocess.DEVNULL)
    if result.returncode not in (0, 2):
        raise SystemExit('BCDへの値追加に失敗しました')
    new = export(candidate, new_export)
    # reged may order new values after existing values in the same key.
    if new.replace('"System"=dword:00000001\n', '', 1) != old or new.count('"System"=dword:00000001\n') != 1:
        raise SystemExit('BCDに想定外のレジストリ変更があります')
    with a.output.open('xb') as out:
        out.write(candidate.read_bytes())
print('BCD SHA-256:', hashlib.sha256(a.output.read_bytes()).hexdigest())
