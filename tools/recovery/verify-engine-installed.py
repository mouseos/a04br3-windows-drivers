#!/usr/bin/env python3
"""Read-only, hash-pinned post-update SYS comparison. Never load a hive or access a device."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(media_path, media_sha, backup_path, backup_sha, root):
    root = root.resolve(strict=True)
    for path, pin in ((media_path, media_sha), (backup_path, backup_sha)):
        if not path.resolve(strict=True).is_relative_to(root) or sha(path) != pin.lower():
            raise ValueError('Input manifest path/hash mismatch')
    media, backup = [json.loads(p.read_text()) for p in (media_path, backup_path)]
    if (media.get('audio_mode') != 'engine_pcm_retained' or backup.get('status') != 'captured-unmounted'
            or backup_sha.lower() == media.get('host_backup_manifest_sha256')):
        raise ValueError('A new completed post-update backup is required')
    expected = media.get('expected_installed_sys', {})
    if not {'mt8167-shared.sys', 'mt8167-audio.sys', 'mt8167-usb20.sys', 'mt6630-runtime.sys'}.issubset(expected):
        raise ValueError('Expected fixed SYS source manifest is incomplete')
    entries = {entry['source_path'].replace('\\', '/').casefold(): entry for entry in backup['files']}
    results = {}
    for name, digest in expected.items():
        if '/' in name or '\\' in name or not name.endswith('.sys'):
            raise ValueError('Unexpected SYS basename')
        entry = entries.get(('Windows/System32/Drivers/' + name).casefold())
        actual = None
        if entry:
            local = (backup_path.parent / entry['backup_path']).resolve(strict=True)
            if not local.is_relative_to(backup_path.parent.resolve()) or not local.is_relative_to(root):
                raise ValueError('Backup file escaped its private directory')
            actual = sha(local)
            if actual != entry['sha256'].lower() or local.stat().st_size != entry['bytes']:
                raise ValueError('Backup SYS content differs from its manifest')
        results[name] = {'expected_sha256': digest.lower(), 'actual_sha256': actual,
                         'matches': actual == digest.lower()}
    passed = bool(results) and all(row['matches'] for row in results.values())
    return {'status': 'all-installed-sys-hashes-match' if passed else 'installed-sys-mismatch',
            'files': results, 'live_loaded_image_verified': False, 'functional_io_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('workspace', 'media', 'backup'):
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ('media-sha256', 'backup-sha256'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    result = verify(args.media, args.media_sha256, args.backup, args.backup_sha256, args.workspace)
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'all-installed-sys-hashes-match' else 1


if __name__ == '__main__':
    raise SystemExit(main())
