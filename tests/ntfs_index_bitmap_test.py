#!/usr/bin/env python3
"""Exercise index bitmap initialization on a disposable small NTFS file."""
import ctypes
import ctypes.util
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/winpe'))
from ntfs_index_bitmap import normalize_index_bitmap

with tempfile.TemporaryDirectory(prefix='a04-index-test-') as tmp:
    image = Path(tmp) / 'volume.img'
    with image.open('wb') as f:
        f.truncate(32 * 1024 * 1024)
    subprocess.run(['mkntfs', '-F', '-Q', str(image)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    size = image.stat().st_size
    assert normalize_index_bitmap(image, size, 0, '/') == (8, 8, 8)
    lib = ctypes.CDLL(ctypes.util.find_library('ntfs-3g'), use_errno=True)
    lib.ntfs_mount.argtypes = [ctypes.c_char_p, ctypes.c_ulong]
    lib.ntfs_mount.restype = ctypes.c_void_p
    lib.ntfs_inode_open.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    lib.ntfs_inode_open.restype = ctypes.c_void_p
    lib.ntfs_attr_open.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
    lib.ntfs_attr_open.restype = ctypes.c_void_p
    lib.ntfs_attr_force_non_resident.argtypes = [ctypes.c_void_p]
    lib.ntfs_attr_force_non_resident.restype = ctypes.c_int
    lib.ntfs_attr_truncate.argtypes = [ctypes.c_void_p, ctypes.c_int64]
    lib.ntfs_attr_truncate.restype = ctypes.c_int
    lib.ntfs_attr_pwrite.argtypes = [ctypes.c_void_p, ctypes.c_int64, ctypes.c_int64, ctypes.c_void_p]
    lib.ntfs_attr_pwrite.restype = ctypes.c_int64
    lib.ntfs_attr_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_inode_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_inode_close.restype = ctypes.c_int
    lib.ntfs_umount.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.ntfs_umount.restype = ctypes.c_int
    volume = lib.ntfs_mount(str(image).encode(), 0)
    assert volume
    inode = lib.ntfs_inode_open(volume, 5)
    assert inode
    name = ctypes.create_string_buffer('$I30'.encode('utf-16le'))
    attr = lib.ntfs_attr_open(inode, 0xb0, name, 4)
    assert attr
    assert lib.ntfs_attr_force_non_resident(attr) == 0
    assert lib.ntfs_attr_truncate(attr, 259) == 0
    zero = ctypes.create_string_buffer(b'\0', 1)
    assert lib.ntfs_attr_pwrite(attr, 258, 1, zero) == 1
    assert lib.ntfs_attr_truncate(attr, 264) == 0
    lib.ntfs_attr_close(attr)
    assert lib.ntfs_inode_close(inode) == 0
    assert lib.ntfs_umount(volume, 0) == 0
    assert normalize_index_bitmap(image, size, 0, '/') == (264, 259, 264)
    assert normalize_index_bitmap(image, size, 0, '/') == (264, 264, 264)
    link = Path(tmp) / 'link.img'
    link.symlink_to(image)
    for path, length, start in ((link, size, 0), (image, size + 1, 0), (image, size, 1)):
        try:
            normalize_index_bitmap(path, length, start, '/')
        except ValueError:
            continue
        raise AssertionError('Expected unsafe image input to be rejected')
print('NTFS index bitmap image tests passed')
