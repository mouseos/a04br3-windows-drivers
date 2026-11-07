"""Validate a private pre-update fixed MT6630 snapshot; no hive/device access."""
import json
import re
from private_hardware_probe_input import sha
from decode_mt6630_checkpoint_archive import decode_record


def validate_archive(path, backup_path, backup):
    if path.stat().st_size > 65536:
        raise ValueError('旧WiFi記録が上限超過')
    d = json.loads(path.read_text(encoding='utf-8-sig'))
    if (d.get('schema') != 1 or d.get('backup_manifest_sha256') != sha(backup_path)
            or d.get('status') != 'private-replayed-checkpoints-only'
            or d.get('native_result') != {'load_rc': 0, 'unload_rc': 0, 'privilege_restore_error': 0}
            or d.get('original_input_unchanged') is not True):
        raise ValueError('旧WiFi記録のfresh/native binding不一致')
    entries = {e['source_path'].replace('\\', '/'): e for e in backup['files']}
    expected = {name: {k: entries['Windows/System32/config/'+name][k] for k in ('bytes', 'sha256')}
                for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2')}
    if d.get('source_files') != expected:
        raise ValueError('旧WiFi記録と固定基点の元入力不一致')
    c = d.get('checkpoint', {})
    if c.get('present') is False:
        return d
    if c.get('present') is not True or set(c.get('records', {})) != {'DriverEntry', 'Initialize', 'Scan', 'RuntimeFailure'}:
        raise ValueError('固定4channel記録が必要です')
    for channel, name in enumerate(('DriverEntry', 'Initialize', 'Scan', 'RuntimeFailure')):
        e = c['records'][name]
        if e.get('present') is False:
            continue
        raw = e.get('hex', '')
        if (e.get('present') is not True or e.get('kind') != 'Binary' or e.get('bytes') != 1624
                or e.get('channel') != channel or not re.fullmatch('[0-9a-fA-F]{3248}', raw)):
            raise ValueError('固定checkpointのREG_BINARY/1624B/channel不一致')
        record = decode_record(bytes.fromhex(raw))
        if (record['channel'] != name or record['generation'] != e.get('generation')
                or record['parent_generation'] != e.get('parent_generation')):
            raise ValueError('checkpoint wireとnative metadata不一致')
    return d
