"""Normalize the root attributes in a newly built regular-file NTFS image.

Uses libntfs-3g and refuses block devices, symlinks, unexpected geometry or an
unexpected root state. This is for freshly generated images, not live disks.
"""
import ctypes
import ctypes.util
import os
from pathlib import Path
import stat
import struct


def normalize_root(path: Path, expected_size: int, expected_start: int) -> tuple[int, int]:
    path = Path(path)
    st = path.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_size != expected_size:
        raise ValueError('Expected a regular image file with the specified size')
    with path.open('rb') as f:
        boot = f.read(512)
    if (len(boot) != 512 or boot[3:11] != b'NTFS    ' or
            struct.unpack_from('<H', boot, 11)[0] != 512 or
            struct.unpack_from('<I', boot, 28)[0] != expected_start or boot[510:512] != b'\x55\xaa'):
        raise ValueError('Unexpected NTFS image geometry')
    name = ctypes.util.find_library('ntfs-3g')
    if not name:
        raise RuntimeError('libntfs-3g is required')
    lib = ctypes.CDLL(name, use_errno=True)
    lib.ntfs_mount.argtypes = [ctypes.c_char_p, ctypes.c_ulong]
    lib.ntfs_mount.restype = ctypes.c_void_p
    lib.ntfs_inode_open.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    lib.ntfs_inode_open.restype = ctypes.c_void_p
    lib.ntfs_attr_open.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
    lib.ntfs_attr_open.restype = ctypes.c_void_p
    lib.ntfs_attr_pread.argtypes = [ctypes.c_void_p, ctypes.c_int64, ctypes.c_int64, ctypes.c_void_p]
    lib.ntfs_attr_pread.restype = ctypes.c_int64
    lib.ntfs_attr_pwrite.argtypes = lib.ntfs_attr_pread.argtypes
    lib.ntfs_attr_pwrite.restype = ctypes.c_int64
    lib.ntfs_attr_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_inode_close.argtypes = [ctypes.c_void_p]
    lib.ntfs_inode_close.restype = ctypes.c_int
    lib.ntfs_umount.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.ntfs_umount.restype = ctypes.c_int
    volume = lib.ntfs_mount(os.fsencode(path.resolve()), 0)
    if not volume:
        raise OSError(ctypes.get_errno(), 'Unable to open NTFS image')
    inode = attr = None
    failure = None
    try:
        inode = lib.ntfs_inode_open(volume, 5)
        if not inode:
            raise OSError(ctypes.get_errno(), 'Unable to open NTFS root')
        attr = lib.ntfs_attr_open(inode, 0x10, None, 0)
        if not attr:
            raise OSError(ctypes.get_errno(), 'Unable to open root standard information')
        value = ctypes.create_string_buffer(4)
        if lib.ntfs_attr_pread(attr, 32, 4, value) != 4:
            raise OSError(ctypes.get_errno(), 'Unable to read root attributes')
        before = struct.unpack('<I', value.raw)[0]
        if before not in (0, 6, 0x26):
            raise ValueError(f'Unexpected NTFS root attributes: 0x{before:x}')
        after = before | 6
        value.raw = struct.pack('<I', after)
        if after != before and lib.ntfs_attr_pwrite(attr, 32, 4, value) != 4:
            raise OSError(ctypes.get_errno(), 'Unable to update root attributes')
        check = ctypes.create_string_buffer(4)
        if lib.ntfs_attr_pread(attr, 32, 4, check) != 4 or struct.unpack('<I', check.raw)[0] != after:
            raise OSError('Root attribute verification failed')
    except BaseException as exc:
        failure = exc
    finally:
        if attr:
            lib.ntfs_attr_close(attr)
        if inode and lib.ntfs_inode_close(inode) != 0 and failure is None:
            failure = OSError(ctypes.get_errno(), 'Unable to close NTFS root')
        if lib.ntfs_umount(volume, 0) != 0 and failure is None:
            failure = OSError(ctypes.get_errno(), 'Unable to close NTFS image')
    if failure:
        raise failure
    return before, after
