#!/usr/bin/env python3
"""Capture private Windows pre-update files read-only; never flash or reboot.

The verify subcommand is a read-only gate for a separate writer. It cannot
prove that another Recovery process did not modify Windows in the meantime.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import shlex
import signal
import subprocess
import sys
import uuid

WORKSPACE = Path(__file__).resolve().parents[4]
PARTITION = '/dev/block/mmcblk0p20'
BY_NAME = '/dev/block/by-name/Windows'
START_SECTORS = 917504
SIZE_SECTORS = 29584384
CONFIG = 'Windows/System32/config/'
DRIVERS = 'Windows/System32/Drivers/'
ENGINE_STATE = ('Windows/System32/MT8167EngineProbe/attempted.bin',
                'Windows/System32/MT8167EngineProbe/logs/service.log',
                'Windows/System32/MT8167EngineProbe.exe')
ENGINE_OPTIONAL_STATE = ('Windows/System32/MT8167CombinedFollowup132.exe',
'Windows/System32/MT8167CombinedFollowup130.exe',
'Windows/System32/MT8167CombinedFollowup128.exe',
'Windows/System32/MT8167CombinedFollowup126.exe',
'Windows/System32/MT8167CombinedFollowup124.exe',
                         'MT8167EngineProbe/brightness-native.log',
                         'Windows/System32/MT8167EngineProbe/logs/brightness-native.log',
                         'Windows/System32/MT8167CaptureProbe/capture.log',
                         'Windows/System32/MT8167EngineProbe/logs/capture.stdout.txt',
                         'Windows/System32/MT8167EngineProbe/logs/capture.stderr.txt',
                         'Windows/System32/MT8167EngineProbe/logs/reboot.log',)
HIVES = ('SYSTEM', 'SOFTWARE', 'DRIVERS')
UUID = re.compile(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}')
SHA256 = re.compile(r'[0-9a-f]{64}')
SYS_NAME = re.compile(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,120}\.sys', re.I)
MAX_FILE_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 1024 * 1024 * 1024


class BackupError(RuntimeError):
    pass


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def inside_workspace(path, existing=False):
    root = WORKSPACE.resolve(strict=True)
    if not (root / 'windows-rt/AGENTS.md').is_file() or not (root / 'port').is_dir():
        raise BackupError('Cannot identify the required TAB-A04-BR3 workspace')
    resolved = path.resolve(strict=existing)
    if resolved == root or not resolved.is_relative_to(root):
        raise BackupError('All private host artifacts must remain inside the workspace')
    return resolved


def unescape_mount(value):
    return re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), value)


def parse_mounts(text):
    result = []
    for line in text.splitlines():
        fields = line.split()
        try:
            split = fields.index('-')
            if split < 6 or len(fields) <= split + 3:
                raise ValueError()
            result.append({'device': fields[2], 'mount': unescape_mount(fields[4]),
                           'options': fields[5].split(','), 'fs': fields[split + 1],
                           'source': unescape_mount(fields[split + 2]),
                           'super_options': fields[split + 3].split(',')})
        except (ValueError, IndexError):
            raise BackupError('Malformed Recovery mountinfo') from None
    return result


class Recovery:
    def __init__(self, serial):
        if not serial or not re.fullmatch(r'[A-Za-z0-9_.:-]{1,128}', serial):
            raise BackupError('An explicit valid target serial is required')
        self.serial = serial

    def adb(self, *args, label='adb', timeout=120):
        # Always pin the supplied serial. Never enumerate/select another device.
        try:
            p = subprocess.run(['adb', '-s', self.serial, *args], timeout=timeout,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except (OSError, subprocess.TimeoutExpired):
            raise BackupError(label + ': adb unavailable or timed out') from None
        if p.returncode:
            # Do not publish adb stderr, which may contain the serial or private paths.
            raise BackupError(label + ': adb exit ' + str(p.returncode))
        return p.stdout.decode('utf-8', errors='strict').strip()

    def shell(self, command, label='shell', timeout=60):
        return self.adb('shell', '-T', command, label=label, timeout=timeout)

    def identity(self):
        if self.adb('get-state', label='Recovery state') != 'recovery':
            raise BackupError('The pinned target is not in Recovery')
        if self.adb('get-serialno', label='serial identity') != self.serial:
            raise BackupError('Pinned serial identity mismatch')
        if self.shell('id -u', label='Recovery UID') != '0':
            raise BackupError('Recovery UID must be zero')
        boot = self.shell('cat /proc/sys/kernel/random/boot_id', label='Recovery boot ID').lower()
        if not UUID.fullmatch(boot):
            raise BackupError('Invalid Recovery boot ID')
        target = self.shell('readlink -f ' + BY_NAME, label='Windows partition alias')
        start = self.shell('cat /sys/class/block/mmcblk0p20/start', label='Windows partition start')
        size = self.shell('cat /sys/class/block/mmcblk0p20/size', label='Windows partition size')
        device = self.shell('cat /sys/class/block/mmcblk0p20/dev', label='Windows partition device')
        if target != PARTITION or start != str(START_SECTORS) or size != str(SIZE_SECTORS):
            raise BackupError('Windows partition layout mismatch')
        if not re.fullmatch(r'\d+:\d+', device):
            raise BackupError('Invalid Windows partition device identity')
        return {'serial': self.serial, 'recovery_boot_id': boot,
                'source_partition': target, 'source_partition_start_sectors': START_SECTORS,
                'source_partition_size_sectors': SIZE_SECTORS,
                'source_partition_major_minor': device}

    def mounts(self):
        return parse_mounts(self.shell('cat /proc/self/mountinfo', label='mountinfo'))

    def windows_mounts(self, identity):
        result = []
        for entry in self.mounts():
            matches = entry['device'] == identity['source_partition_major_minor']
            if entry['source'] in (BY_NAME, PARTITION):
                matches = True
            elif entry['source'].startswith('/dev/'):
                # FUSE mounts have a different major:minor; resolve source aliases too.
                resolved = self.shell('readlink -f ' + shlex.quote(entry['source']), label='mount source alias')
                matches = matches or resolved == PARTITION
            if matches:
                result.append(entry)
        return result

    def require_unmounted(self, identity):
        if self.windows_mounts(identity):
            raise BackupError('Windows is already mounted; do not disturb the existing mount')

    def require_own_ro_mount(self, mount, identity):
        entries = self.windows_mounts(identity)
        if len(entries) != 1 or entries[0]['mount'] != mount:
            raise BackupError('The exclusive Windows mount cannot be established')
        entry = entries[0]
        if ('ro' not in entry['options'] or 'rw' in entry['options'] or
                'ro' not in entry['super_options'] or 'rw' in entry['super_options'] or
                entry['fs'] not in ('ntfs', 'fuseblk', 'fuse.ntfs-3g')):
            raise BackupError('The Windows mount is not verified read-only NTFS')

    def source_stat(self, mount, source, optional=False):
        # test -e/-f collapse EIO/EACCES into false. Only stat's explicit
        # ENOENT diagnostic in the C locale authorizes an optional absence.
        # Capture rc in-band so adb transport failures still fail separately.
        path = shlex.quote(mount + '/' + source)
        output = self.shell("LC_ALL=C; export LC_ALL; stat -c '%f %s' -- " + path +
                            ' 2>&1; rc=$?; printf "\\nA04_STAT_RC=%s\\n" "$rc"',
                            label='source path stat')
        body, separator, status = output.rpartition('\nA04_STAT_RC=')
        if not separator or not status.isdigit():
            raise BackupError('Invalid source stat protocol: ' + source)
        body = body.strip()
        if int(status):
            if (optional and len(body.splitlines()) == 1 and
                    body.startswith('stat: ') and body.endswith(': No such file or directory')):
                return None
            reason = 'I/O error' if body.endswith(': Input/output error') else 'stat error (not absence)'
            raise BackupError('Source ' + reason + ': ' + source)
        if not re.fullmatch(r'[0-9a-fA-F]+ [0-9]+', body):
            raise BackupError('Invalid source stat metadata: ' + source)
        mode, size = body.split()
        return int(mode, 16) & 0xf000, int(size)

    def snapshot(self, mount, specs):
        directories={'Windows', 'Windows/System32', CONFIG.rstrip('/'), DRIVERS.rstrip('/')}
        for source, required in specs:
            if required:
                directories.update(str(parent) for parent in PurePosixPath(source).parents if str(parent) != '.')
        optional_parents={str(parent) for source,required in specs if not required
                          for parent in PurePosixPath(source).parents if str(parent)!='.'}
        # Validate each ancestor itself, without listing unrelated siblings.
        # A dangling symlink is rejected by mode, not treated as ENOENT.
        for directory in sorted(directories | optional_parents,
                                key=lambda p:(len(PurePosixPath(p).parts),p)):
            metadata = self.source_stat(mount, directory, directory not in directories)
            if metadata is not None and metadata[0] != 0x4000:
                raise BackupError('Source parent is not a real directory: ' + directory)
        result = {}
        for source, required in specs:
            metadata = self.source_stat(mount, source, not required)
            if metadata is None:
                result[source] = None
                continue
            mode, size = metadata
            if mode != 0x8000:
                raise BackupError('Source is not a regular non-symlink file: ' + source)
            if size > MAX_FILE_BYTES:
                raise BackupError('Source bounded size check failed')
            if source in {CONFIG + hive for hive in HIVES} and size == 0:
                raise BackupError('A required hive is empty')
            path = shlex.quote(mount + '/' + source)
            data = self.shell('sha256sum ' + path, label='source file hash').splitlines()
            if len(data) != 1 or not data[0].split():
                raise BackupError('Invalid source hash output')
            digest = data[0].split()[0].lower()
            if not SHA256.fullmatch(digest):
                raise BackupError('Source hash check failed')
            result[source] = {'bytes': size, 'sha256': digest}
        if sum(x['bytes'] for x in result.values() if x) > MAX_TOTAL_BYTES:
            raise BackupError('Backup size limit exceeded')
        return result


def file_specs(sys_names, allow_missing_sys, engine_state=False, allow_missing_engine_log=False, sd_component_state=False):
    if allow_missing_engine_log and not engine_state:
        raise BackupError("Missing Engine log capture requires explicit --engine-probe-state")
    names = []
    for name in sys_names:
        if not SYS_NAME.fullmatch(name) or name.casefold() in [x.casefold() for x in names]:
            raise BackupError('Target SYS must be a unique driver basename, not a path')
        names.append(name)
    specs = [(CONFIG + hive, True) for hive in HIVES]
    specs += [(CONFIG + hive + suffix, True) for hive in HIVES for suffix in ('.LOG1', '.LOG2')]
    if sd_component_state and 'mt8167-sd.sys' not in [n.casefold() for n in names]:specs.append((DRIVERS+'mt8167-sd.sys',False))
    return specs + [(DRIVERS + name, not allow_missing_sys) for name in names] + ([(name, not (allow_missing_engine_log and name == ENGINE_STATE[1])) for name in ENGINE_STATE]+[(name,False) for name in ENGINE_OPTIONAL_STATE] if engine_state else [])


def write_manifest(out, manifest):
    path = out / 'manifest.json'
    with path.open('x', encoding='utf-8') as f:
        os.chmod(path, 0o600)
        json.dump(manifest, f, ensure_ascii=False, indent=2)
        f.write('\n'); f.flush(); os.fsync(f.fileno())
    return path


def capture(args, recovery=None):
    out = inside_workspace(args.out)
    # An existing parent is required; the new private directory is never reused.
    if not out.parent.is_dir():
        raise BackupError('The host backup parent must already exist')
    compare_media=getattr(args,'compare_engine_media',None)
    compare_sha=getattr(args,'compare_engine_media_sha256',None)
    if bool(compare_media)!=bool(compare_sha):raise BackupError('Engine media comparison requires both manifest and SHA256')
    specs = file_specs(args.target_sys, args.allow_missing_sys, getattr(args, 'engine_probe_state', False), getattr(args, 'allow_missing_engine_service_log', False), getattr(args,'sd_component_state',False))
    recovery = recovery or Recovery(args.serial)
    out.mkdir(mode=0o700)
    os.chmod(out, 0o700)
    mount = '/tmp/mt8167-host-backup-' + uuid.uuid4().hex
    manifest = {'schema_version': 1, 'status': 'capture-failed', 'files': [], 'missing_files': [],
                'captured_at_utc': datetime.now(timezone.utc).isoformat(),
                'capture_tool_sha256': sha(Path(__file__).resolve()),
                'sd_component_state': bool(getattr(args,'sd_component_state',False)),
                'engine_missing_service_log_opt_in': bool(getattr(args, 'allow_missing_engine_service_log', False)),
                'validity': 'same Recovery boot only; no intervening Windows boot or Windows mutation',
                'read_only': True, 'unmounted': False, 'errors': [],
                'source_probe_scope': 'requested files and ancestors only; siblings not enumerated or verified'}
    mount_attempted = False
    made_mount_directory = False
    before = None
    try:
        identity = recovery.identity(); manifest.update(identity)
        recovery.require_unmounted(identity)
        recovery.shell('test -d /tmp && test ! -L /tmp && mkdir ' + shlex.quote(mount), label='private RAM mount directory')
        made_mount_directory = True
        if args.mount_driver == 'ntfs-3g':
            binary = PurePosixPath(args.ntfs3g_remote)
            if not binary.is_absolute() or '..' in binary.parts or '\n' in str(binary):
                raise BackupError('Use an existing absolute Recovery ntfs-3g path')
            recovery.shell('test -x ' + shlex.quote(str(binary)), label='resident NTFS driver')
            command = shlex.quote(str(binary)) + ' -o ro,norecover,nosuid,nodev,noexec '
        else:
            command = 'mount -t ntfs -o ro,nosuid,nodev,noexec '
        mount_attempted = True
        recovery.shell(command + BY_NAME + ' ' + shlex.quote(mount), label='read-only Windows mount')
        recovery.require_own_ro_mount(mount, identity)
        before = recovery.snapshot(mount, specs)
        total = sum(x['bytes'] for x in before.values() if x)
        if shutil.disk_usage(out).free < total + 16 * 1024 * 1024:
            raise BackupError('Insufficient host workspace space')
        for source, metadata in before.items():
            if metadata is None:
                manifest['missing_files'].append({'source_path': source, 'reason': 'absent'})
                continue
            relative = 'files/' + source
            local = out / relative
            local.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            recovery.adb('pull', mount + '/' + source, str(local), label='private file pull', timeout=300)
            if local.is_symlink() or not local.is_file():
                raise BackupError('The pulled file is not a regular host file')
            os.chmod(local, 0o600)
            if local.stat().st_size != metadata['bytes'] or sha(local) != metadata['sha256']:
                raise BackupError('Pulled file length or SHA-256 mismatch')
            manifest['files'].append({'source_path': source, 'backup_path': relative, **metadata})
        recovery.require_own_ro_mount(mount, identity)
        if recovery.snapshot(mount, specs) != before or recovery.identity() != identity:
            raise BackupError('The source snapshot or Recovery boot changed during capture')
    except (BackupError, OSError, UnicodeError, KeyboardInterrupt) as e:
        manifest['errors'].append(str(e) or 'Capture interrupted')
    finally:
        if mount_attempted:
            try:
                # Always attempt normal unmount, including a failed/partial mount command.
                recovery.shell('umount ' + shlex.quote(mount), label='Windows unmount')
            except (BackupError, OSError, UnicodeError):
                manifest['errors'].append('Unmount command did not complete successfully')
            try:
                if any(x['mount'] == mount for x in recovery.mounts()):
                    raise BackupError('The private Windows mount remains mounted')
                if 'source_partition' in manifest:
                    recovery.require_unmounted(manifest)
                manifest['unmounted'] = True
            except (BackupError, OSError, UnicodeError) as e:
                manifest['errors'].append(str(e))
        if made_mount_directory and not mount_attempted:
            try:
                recovery.require_unmounted(manifest)
                manifest['unmounted'] = True
            except (BackupError, OSError, UnicodeError) as e:
                manifest['errors'].append(str(e))
        if made_mount_directory and manifest['unmounted']:
            try:
                recovery.shell('rmdir ' + shlex.quote(mount), label='RAM mount directory removal')
            except (BackupError, OSError, UnicodeError):
                manifest['errors'].append('RAM mount directory cleanup failed')
    if not manifest['errors'] and manifest['unmounted'] and before is not None:
        manifest['status'] = 'captured-unmounted'
    path = write_manifest(out, manifest)
    # No serial, partition, boot ID or file contents on stdout.
    print(json.dumps({'status': manifest['status'], 'manifest_sha256': sha(path)}))
    if compare_media and manifest['status']=='captured-unmounted':
        spec=importlib.util.spec_from_file_location('engine_installed',Path(__file__).with_name('verify-engine-installed.py'))
        verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
        result=verifier.verify(compare_media,compare_sha,path,sha(path),WORKSPACE)
        result_path=out/'engine-installed-verification.json'
        with result_path.open('x') as stream:
            os.chmod(result_path,0o600);json.dump(result,stream,indent=2);stream.write('\n')
        print(json.dumps({'engine_installed_status':result['status'],'functional_io_verified':False}))
        if result['status']!='all-installed-sys-hashes-match':return 3
    return 0 if manifest['status'] == 'captured-unmounted' else 2


def verify_manifest(args, recovery=None):
    path = inside_workspace(args.manifest, existing=True)
    if path.stat().st_size > 1024 * 1024:
        raise BackupError('The private manifest is too large')
    raw = path.read_bytes()
    if not SHA256.fullmatch(args.manifest_sha256) or hashlib.sha256(raw).hexdigest() != args.manifest_sha256:
        raise BackupError('The pinned private manifest hash does not match')
    manifest = json.loads(raw)
    if not isinstance(manifest, dict) or not isinstance(manifest.get('files'), list):
        raise BackupError('Invalid private manifest structure')
    if (manifest.get('schema_version') != 1 or manifest.get('status') != 'captured-unmounted' or
            manifest.get('unmounted') is not True or manifest.get('read_only') is not True or
            manifest.get('errors') != [] or manifest.get('serial') != args.serial):
        raise BackupError('The private manifest is not an eligible completed backup')
    seen = set()
    for item in manifest['files']:
        source = item['source_path']
        allowed_config = {CONFIG + hive + suffix for hive in HIVES for suffix in ('', '.LOG1', '.LOG2')}
        if not (source in allowed_config or source in ENGINE_STATE or source in ENGINE_OPTIONAL_STATE or (source.startswith(DRIVERS) and SYS_NAME.fullmatch(source[len(DRIVERS):]))):
            raise BackupError('Unexpected source path in pinned manifest')
        if item['backup_path'] != 'files/' + source or source in seen:
            raise BackupError('Invalid or repeated backup path')
        seen.add(source)
        local = inside_workspace(path.parent / item['backup_path'], existing=True)
        if not local.is_relative_to(path.parent.resolve()) or local.is_symlink() or not local.is_file():
            raise BackupError('Private backup path escaped its snapshot directory')
        if local.stat().st_size != item['bytes'] or sha(local) != item['sha256']:
            raise BackupError('A private backup file was modified or lost')
    required = {CONFIG + hive + suffix for hive in HIVES for suffix in ('', '.LOG1', '.LOG2')}
    if not required.issubset(seen):
        raise BackupError('The required SYSTEM/SOFTWARE/DRIVERS hive and log set is missing')
    recovery = recovery or Recovery(args.serial)
    identity = recovery.identity()
    if any(manifest.get(k) != value for k, value in identity.items()):
        raise BackupError('Recovery boot ID, target identity or partition layout changed; capture again')
    recovery.require_unmounted(identity)
    print(json.dumps({'status': 'verified-same-recovery-unmounted', 'manifest_sha256': args.manifest_sha256}))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    capture_parser = sub.add_parser('capture', help='capture a new private read-only snapshot')
    capture_parser.add_argument('--serial', required=True)
    capture_parser.add_argument('--out', type=Path, required=True)
    capture_parser.add_argument('--target-sys', action='append', required=True)
    capture_parser.add_argument('--allow-missing-sys', action='store_true')
    capture_parser.add_argument('--compare-engine-media',type=Path)
    capture_parser.add_argument('--compare-engine-media-sha256')
    capture_parser.add_argument('--allow-missing-engine-service-log', action='store_true', help='explicit missing-log snapshot only; Engine marker and EXE remain required')
    capture_parser.add_argument('--sd-component-state', action='store_true', help='snapshot only the fixed optional mt8167-sd.sys, with explicit ENOENT; other required SYS remain required')
    capture_parser.add_argument('--engine-probe-state', action='store_true', help='capture fixed Engine marker/log/EXE plus optional fixed brightness root log for explicit rearm')
    capture_parser.add_argument('--mount-driver', choices=('kernel-ntfs', 'ntfs-3g'), default='kernel-ntfs')
    capture_parser.add_argument('--ntfs3g-remote', default='/sbin/ntfs-3g', help='resident binary only; never pushed')
    verify_parser = sub.add_parser('verify', help='read-only writer preflight; performs no write')
    verify_parser.add_argument('--serial', required=True)
    verify_parser.add_argument('--manifest', type=Path, required=True)
    verify_parser.add_argument('--manifest-sha256', required=True)
    args = parser.parse_args()
    def interrupted(signum, frame):
        raise BackupError('Operation interrupted by signal ' + str(signum))
    signal.signal(signal.SIGTERM, interrupted)
    try:
        return capture(args) if args.mode == 'capture' else verify_manifest(args)
    except BackupError as e:
        print('Prebackup refused: ' + str(e), file=sys.stderr)
        return 2
    except (OSError, ValueError, KeyError, TypeError):
        print('Prebackup refused: local I/O or private manifest validation failed', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
