#!/usr/bin/env python3
"""失敗済み実験OSのセットアップ開始状態を、検証済みの最小値に戻す。"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--system', type=Path, required=True)
p.add_argument('--software', type=Path, required=True)
p.add_argument('--state', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
a = p.parse_args()
if a.output_dir.exists() or any(not f.is_file() for f in (a.system, a.software, a.state)):
    raise SystemExit('入力がないか出力ディレクトリが既にあります')

def prepare(src, dst, prefix, key, before, after):
    with tempfile.TemporaryDirectory(prefix='a04-setup-') as directory:
        d = Path(directory)
        candidate = d / 'hive'
        shutil.copyfile(src, candidate)
        def export(path, name):
            out = d / name
            subprocess.run(['reged', '-x', str(path), prefix, key, str(out)],
                           check=True, stdout=subprocess.DEVNULL)
            return out.read_text(encoding='utf-8')
        old = export(candidate, 'old.reg')
        if old.count(before) != 1:
            raise SystemExit('対象の元値が想定と一致しません')
        patch = d / 'change.reg'
        patch.write_text('Windows Registry Editor Version 5.00\n\n' +
                         f'[{prefix}{key}]\n' + after + '\n', encoding='ascii')
        rc = subprocess.run(['reged', '-C', '-I', str(candidate), prefix, str(patch)],
                            stdout=subprocess.DEVNULL).returncode
        if rc not in (0, 2) or export(candidate, 'new.reg') != old.replace(before, after, 1):
            raise SystemExit('想定外のレジストリ差分があります')
        shutil.copyfile(candidate, dst)

state = a.state.read_bytes()
old_state = b'IMAGE_STATE_UNDEPLOYABLE'
new_state = b'IMAGE_STATE_GENERALIZE_RESEAL_TO_OOBE'
if state.count(old_state) != 1:
    raise SystemExit('State.iniの元値が想定と一致しません')
a.output_dir.mkdir(parents=True)
prepare(a.system, a.output_dir / 'SYSTEM', r'HKEY_LOCAL_MACHINE\SYSTEM',
        r'\Setup\Status\ChildCompletion', '"setup.exe"=dword:00000001',
        '"setup.exe"=dword:00000000')
prepare(a.software, a.output_dir / 'SOFTWARE', r'HKEY_LOCAL_MACHINE\SOFTWARE',
        r'\Microsoft\Windows\CurrentVersion\Setup\State',
        '"ImageState"="IMAGE_STATE_UNDEPLOYABLE"',
        '"ImageState"="IMAGE_STATE_GENERALIZE_RESEAL_TO_OOBE"')
(a.output_dir / 'State.ini').write_bytes(state.replace(old_state, new_state, 1))
print('セットアップ再試行用の候補を作成しました。元ハイブは変更していません。')
