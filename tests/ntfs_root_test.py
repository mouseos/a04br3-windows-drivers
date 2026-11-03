#!/usr/bin/env python3
"""Exercise root-attribute normalization only on a disposable NTFS file."""
import ctypes
import ctypes.util
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/winpe'))
from ntfs_root_attributes import normalize_root

with tempfile.TemporaryDirectory(prefix='a04-ntfs-test-') as tmp:
    image = Path(tmp) / 'volume.img'
    with image.open('wb') as f:
        f.truncate(32 * 1024 * 1024)
    subprocess.run(['mkntfs', '-F', '-Q', str(image)], check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    size = image.stat().st_size
    lib = ctypes.CDLL(ctypes.util.find_library('ntfs-3g'), use_errno=True)
    lib.ntfs_mount.argtypes = [ctypes.c_char_p, ctypes.c_ulong]
    lib.ntfs_mount.restype = ctypes.c_void_p
    lib.ntfs_inode_open.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    lib.ntfs_inode_open.restype = ctypes.c_void_p
    lib.ntfs_attr_open.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
    lib.ntfs_attr_open.restype = ctypes.c_void_p
    lib.ntfs_attr_pwrite.argtypes = [ctypes.c_void_p, ctypes.c_int64, ctypes.c_int64, ctypes.c_void_p]
    lib.ntfs_attr_pwrite.restype = ctypes.c_int64
    lib.ntfs_attr_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_inode_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_umount.argtypes = [ctypes.c_void_p, ctypes.c_int]
    volume = lib.ntfs_mount(str(image).encode(), 0)
    assert volume
    inode = lib.ntfs_inode_open(volume, 5)
    assert inode
    attr = lib.ntfs_attr_open(inode, 0x10, None, 0)
    assert attr
    zero = ctypes.create_string_buffer(b'\0' * 4, 4)
    assert lib.ntfs_attr_pwrite(attr, 32, 4, zero) == 4
    lib.ntfs_attr_close(attr)
    assert lib.ntfs_inode_close(inode) == 0
    assert lib.ntfs_umount(volume, 0) == 0
    assert normalize_root(image, size, 0) == (0, 6)
    assert normalize_root(image, size, 0) == (6, 6)
    output = subprocess.run(['ntfsinfo', '-i', '5', str(image)], check=True, text=True,
                            stdout=subprocess.PIPE).stdout
    assert 'HIDDEN SYSTEM (0x00000006)' in output
    link = Path(tmp) / 'link.img'
    link.symlink_to(image)
    for path, length, start in ((link, size, 0), (image, size + 1, 0), (image, size, 1)):
        try:
            normalize_root(path, length, start)
        except ValueError:
            continue
        raise AssertionError('Expected unsafe image input to be rejected')
print('NTFS root image tests passed')
