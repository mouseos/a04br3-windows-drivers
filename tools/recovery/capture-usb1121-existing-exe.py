#!/usr/bin/env python3
"""既存USB1121サービスの固定EXEだけを同Recoveryのread-only witnessとして補足する。"""
import argparse
import importlib.util
import json
import shlex
import uuid
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'prebackup', Path(__file__).with_name('capture-windows-prebackup.py'))
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
SERVICE = 'MT8167Usb1121Install198'
EXE = 'Windows/System32/' + SERVICE + '.exe'
MAX_EXE_BYTES = 16 * 1024 * 1024
SYSTEM_TRIO = {c.CONFIG + name for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2')}


def require(ok, message):
    if not ok:
        raise c.BackupError(message)


def parser():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--manifest', type=Path, required=True)
    ap.add_argument('--manifest-sha256', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--include-usb1121-existing-exe', action='store_true',
                    help='既存USB1121固定EXEのlogical copy/hashを明示許可。既定OFF')
    return ap


def main():
    ap = parser()
    a = ap.parse_args()
    if not a.include_usb1121_existing_exe:
        ap.error('固定EXE捕捉には --include-usb1121-existing-exe が必要（既定OFF）')
    mf = c.inside_workspace(a.manifest, True)
    out = c.inside_workspace(a.out)
    require(not out.exists(), '新規root内出力のみ')
    require(c.SHA256.fullmatch(a.manifest_sha256) and c.sha(mf) == a.manifest_sha256,
            '原capture pin不一致')
    m = json.loads(mf.read_text(encoding='utf-8-sig'))
    require(m['status'] == 'captured-unmounted' and m['read_only'] is True and
            m['unmounted'] is True and m['errors'] == [], '完了read-only captureだけ')
    originals = {f['source_path']: f for f in m['files']}
    require(len(originals) == len(m['files']) and SYSTEM_TRIO <= originals.keys(),
            '重複のないSYSTEM trio anchorが必要')
    # 既存preservationと同じく、原capture全ファイルを前後snapshotのanchorにする。
    # 追加のoptionalファイルは固定EXEの1個だけ。既捕捉なら原本hashも照合する。
    specs = [(name, True) for name in originals if name != EXE] + [(EXE, False)]
    r = c.Recovery(m['serial'])
    identity = r.identity()
    require(all(identity[k] == m[k] for k in identity), '同Recovery boot/capture出自必須')
    r.require_unmounted(identity)
    out.mkdir(mode=0o700)
    mount = '/tmp/rt-usb1121-witness-' + uuid.uuid4().hex
    made = mounted = False
    result = dict(schema=1, source_manifest_sha256=a.manifest_sha256,
                  recovery_boot_id=identity['recovery_boot_id'], status='failed',
                  read_only=True, unmounted=False, errors=[], files=[], missing_files=[],
                  usb1121_existing_exe=dict(service=SERVICE, source_path=EXE, present=None,
                      opt_in=True, maximum_bytes=MAX_EXE_BYTES, physical_read_performed=False,
                      loaded_image_proven=False, rearm_admission_proven=False))
    try:
        r.shell('test -d /tmp && test ! -L /tmp && mkdir ' + shlex.quote(mount))
        made = True
        r.shell('test -x /tmp/rt-ntfs3g')
        mounted = True  # mountコマンド失敗時も通常unmountを試みる。
        r.shell('/tmp/rt-ntfs3g -o ro,norecover,nosuid,nodev,noexec ' + c.BY_NAME + ' ' + shlex.quote(mount))
        r.require_own_ro_mount(mount, identity)
        before = r.snapshot(mount, specs)
        for name, old in originals.items():
            require(before[name] == {k: old[k] for k in ('sha256', 'bytes')}, '原capture byte差分')
        witness = before[EXE]
        result['usb1121_existing_exe']['present'] = witness is not None
        if witness is None:
            result['missing_files'].append(dict(source_path=EXE, reason='absent', optional=True))
        else:
            require(0 < witness['bytes'] <= MAX_EXE_BYTES, '固定EXEのサイズ上限/空ファイル')
            local = out / 'files' / EXE
            local.parent.mkdir(parents=True, exist_ok=True)
            r.adb('pull', mount + '/' + EXE, str(local))
            require(not local.is_symlink() and local.is_file() and
                    local.stat().st_size == witness['bytes'] and c.sha(local) == witness['sha256'],
                    '固定EXEのcopy/hash不一致')
            local.chmod(0o600)
            result['files'].append(dict(source_path=EXE, backup_path=local.relative_to(out).as_posix(), **witness))
        r.require_own_ro_mount(mount, identity)
        require(r.snapshot(mount, specs) == before and r.identity() == identity and
                c.sha(mf) == a.manifest_sha256, '同boot/snapshot/原capture不変必須')
    except Exception as error:
        result['errors'].append(str(error))
    finally:
        if mounted:
            try:
                r.shell('umount ' + shlex.quote(mount))
            except Exception as error:
                result['errors'].append(str(error))
        try:
            r.require_unmounted(identity)
            result['unmounted'] = True
        except Exception as error:
            result['errors'].append(str(error))
        if made and result['unmounted']:
            try:
                r.shell('rmdir ' + shlex.quote(mount))
            except Exception as error:
                result['errors'].append(str(error))
    if not result['errors'] and result['unmounted']:
        result['status'] = 'captured-unmounted'
    p = out / 'manifest.json'
    p.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
    p.chmod(0o600)
    print(json.dumps(dict(status=result['status'], manifest_sha256=c.sha(p),
                         usb1121_existing_exe_present=result['usb1121_existing_exe']['present'])))
    if result['status'] != 'captured-unmounted':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
