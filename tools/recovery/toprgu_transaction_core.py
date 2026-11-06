#!/usr/bin/env python3
"""Experimental POSIX fixture core; not a WinPE or device updater.

This module deliberately exposes no command-line entry point. Its directory
fsync contract must be implemented and tested for the target filesystem before
use on a device. Callers must exclusively own the target throughout a run.
"""
import hashlib
import json
import os
from pathlib import Path
import stat

FILES = ('SYSTEM', 'SYSTEM.LOG', 'SYSTEM.LOG1', 'SYSTEM.LOG2', 'mt8167-toprgu.sys')

class TransactionError(RuntimeError):
    pass

def _syncdir(path):
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_DIRECTORY', 0))
    try:
        os.fsync(fd)
    finally:
        os.close(fd)

def _hash(data):
    return hashlib.sha256(data).hexdigest()

def _read_regular(path):
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise TransactionError('not a regular file')
        with os.fdopen(fd, 'rb', closefd=False) as f:
            return f.read()
    finally:
        os.close(fd)

def _write_new(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0), 0o600)
    try:
        with os.fdopen(fd, 'wb', closefd=False) as f:
            f.write(data)
            f.flush()
            os.fsync(fd)
    finally:
        os.close(fd)

def _check_paths(target, journal):
    target, journal = Path(target).resolve(strict=True), Path(journal).resolve(strict=True)
    if not target.is_dir() or not journal.is_dir() or target == journal:
        raise TransactionError('invalid directories')
    if target in journal.parents or journal in target.parents:
        raise TransactionError('journal and target must be separate')
    return target, journal

def prepare(target, journal, fault=lambda phase: None):
    """Persist every original and a completion manifest before mutation.

    A partial journal is never treated as a backup. No destination file is
    touched here. A failed attempt requires inspection, not reuse of the folder.
    """
    target, journal = _check_paths(target, journal)
    if any(journal.iterdir()):
        raise TransactionError('journal is not empty')
    entries = {}
    for name in FILES:
        path = target / name
        try:
            data = _read_regular(path)
        except FileNotFoundError:
            if name in ('SYSTEM', 'mt8167-toprgu.sys'):
                raise TransactionError('required original missing')
            entries[name] = None
            continue
        _write_new(journal / (name + '.original'), data)
        entries[name] = {'size': len(data), 'sha256': _hash(data)}
        fault('backup:' + name)
    _syncdir(journal)
    manifest = json.dumps({'version': 1, 'files': entries}, sort_keys=True, separators=(',', ':')).encode()
    _write_new(journal / 'manifest.pending', manifest)
    os.replace(journal / 'manifest.pending', journal / 'manifest.json')
    _syncdir(journal)
    fault('prepared')

def _load(target, journal):
    target, journal = _check_paths(target, journal)
    try:
        manifest = json.loads(_read_regular(journal / 'manifest.json'))
        entries = manifest['files']
        if manifest['version'] != 1 or set(entries) != set(FILES):
            raise TransactionError('manifest contents rejected')
        originals = {}
        for name in FILES:
            item = entries[name]
            if item is None:
                if name in ('SYSTEM', 'mt8167-toprgu.sys'):
                    raise TransactionError('required original absent')
                originals[name] = None
                continue
            if set(item) != {'size', 'sha256'}:
                raise TransactionError('invalid metadata')
            data = _read_regular(journal / (name + '.original'))
            if len(data) != item['size'] or _hash(data) != item['sha256']:
                raise TransactionError('backup mismatch')
            originals[name] = data
        return target, journal, originals
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise TransactionError('invalid or incomplete backup') from exc

def recover(target, journal, fault=lambda phase: None):
    """Restore all originals; safe to repeat after any interrupted step.

    Validation happens before the first target mutation. Completion does not
    delete the persistent originals or claim that a volume was dismounted.
    """
    target, journal, originals = _load(target, journal)
    for name in FILES:
        data = originals[name]
        path = target / name
        if data is None:
            try:
                info = path.lstat()
            except FileNotFoundError:
                pass
            else:
                if not stat.S_ISREG(info.st_mode):
                    raise TransactionError('unexpected destination type')
                path.unlink()
        else:
            temporary = target / ('.a04-recover-' + name)
            try:
                info = temporary.lstat()
            except FileNotFoundError:
                pass
            else:
                if not stat.S_ISREG(info.st_mode):
                    raise TransactionError('unexpected temporary type')
                temporary.unlink()
            _write_new(temporary, data)
            fault('temporary:' + name)
            os.replace(temporary, path)
        _syncdir(target)
        fault('restored:' + name)
    for name, data in originals.items():
        if data is None:
            if (target / name).exists() or (target / name).is_symlink():
                raise TransactionError('unexpected file remains')
        elif _read_regular(target / name) != data:
            raise TransactionError('restored content mismatch')
    return True
