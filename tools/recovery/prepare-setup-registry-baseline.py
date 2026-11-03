#!/usr/bin/env python3
"""固定された実験NTFSからセットアップ初期レジストリ群を抽出する。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--image', type=Path, required=True)
p.add_argument('--manifest', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
a = p.parse_args()
if a.output_dir.exists():
    raise SystemExit('出力ディレクトリが既にあります')
meta = json.loads(a.manifest.read_text())
h = hashlib.sha256()
with a.image.open('rb') as f:
    for chunk in iter(lambda: f.read(4 * 1024 * 1024), b''):
        h.update(chunk)
if h.hexdigest() != meta.get('sha256') or a.image.stat().st_size != meta.get('size_bytes'):
    raise SystemExit('固定NTFSイメージのハッシュまたはサイズが不一致です')
root = '/Windows/System32/config/'
names = subprocess.check_output(['ntfsls', '-p', root, str(a.image)], text=True).splitlines()
pattern = re.compile(r'^(?:SYSTEM|SOFTWARE|SAM|SECURITY|DEFAULT|COMPONENTS|DRIVERS|BBI|ELAM)(?:$|\.[A-Za-z0-9]+$|\{[0-9A-Fa-f-]+\}\.TM(?:\.blf|Container[0-9]+\.regtrans-ms)$)')
names = sorted(n for n in names if pattern.fullmatch(n))
if not {'SYSTEM', 'SOFTWARE', 'SAM', 'SECURITY', 'DEFAULT', 'COMPONENTS'} <= set(names):
    raise SystemExit('元イメージの必須ハイブが不足しています')
a.output_dir.mkdir(parents=True)
hashes = {}
for name in names:
    out = a.output_dir / name
    with out.open('xb') as f:
        subprocess.run(['ntfscat', str(a.image), root + name], stdout=f, check=True)
    hashes[name] = hashlib.sha256(out.read_bytes()).hexdigest()
with (a.output_dir / 'State.ini').open('xb') as f:
    subprocess.run(['ntfscat', str(a.image), '/Windows/Setup/State/State.ini'], stdout=f, check=True)
hashes['State.ini'] = hashlib.sha256((a.output_dir / 'State.ini').read_bytes()).hexdigest()
(a.output_dir / 'manifest.json').write_text(json.dumps({'source_image_sha256': h.hexdigest(),
    'files': hashes, 'note': '実験用のセットアップ再試行候補。Windowsの復旧を保証しない。'},
    indent=2, ensure_ascii=False) + '\n')
print('抽出完了:', len(names), 'ファイルとState.ini')
