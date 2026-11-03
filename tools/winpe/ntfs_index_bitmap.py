"""Normalize a directory index bitmap in a newly generated NTFS image.

Only regular files with the expected geometry are accepted. Never use this
host-side image builder helper on a live device or an existing installation.
"""
import ctypes
import ctypes.util
import os
from pathlib import Path
import stat
import struct


class _NtfsAttr(ctypes.Structure):
    # Public struct ntfs_attr from libntfs-3g's attrib.h, through initialized_size.
    _fields_ = [
        ('runlist', ctypes.c_void_p), ('inode', ctypes.c_void_p),
        ('type', ctypes.c_uint32), ('data_flags', ctypes.c_uint16),
        ('name', ctypes.c_void_p), ('name_len', ctypes.c_uint32),
        ('state', ctypes.c_ulong), ('allocated_size', ctypes.c_int64),
        ('data_size', ctypes.c_int64), ('initialized_size', ctypes.c_int64),
    ]


def normalize_index_bitmap(path: Path, expected_size: int, expected_start: int,
                           directory: str) -> tuple[int, int, int]:
    path = Path(path)
    st = path.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_size != expected_size:
        raise ValueError('Expected a regular image file with the specified size')
    with path.open('rb') as f:
        boot = f.read(512)
    if (len(boot) != 512 or boot[3:11] != b'NTFS    ' or
            struct.unpack_from('<H', boot, 11)[0] != 512 or
            struct.unpack_from('<I', boot, 28)[0] != expected_start or
            boot[510:512] != b'\x55\xaa'):
        raise ValueError('Unexpected NTFS image geometry')
    if not directory.startswith('/') or '\x00' in directory:
        raise ValueError('Expected an absolute NTFS directory path')
    name = ctypes.util.find_library('ntfs-3g')
    if not name:
        raise RuntimeError('libntfs-3g is required')
    lib = ctypes.CDLL(name, use_errno=True)
    lib.ntfs_mount.argtypes = [ctypes.c_char_p, ctypes.c_ulong]
    lib.ntfs_mount.restype = ctypes.c_void_p
    lib.ntfs_pathname_to_inode.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p]
    lib.ntfs_pathname_to_inode.restype = ctypes.c_void_p
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
        inode = lib.ntfs_pathname_to_inode(volume, None, directory.encode('utf-8'))
        if not inode:
            raise OSError(ctypes.get_errno(), 'Unable to open NTFS directory')
        index_name = ctypes.create_string_buffer('$I30'.encode('utf-16le'))
        attr = lib.ntfs_attr_open(inode, 0xb0, index_name, 4)
        if not attr:
            raise OSError(ctypes.get_errno(), 'Unable to open index bitmap')
        info = ctypes.cast(attr, ctypes.POINTER(_NtfsAttr)).contents
        size, before, allocated = info.data_size, info.initialized_size, info.allocated_size
        if info.type != 0xb0 or not (0 < size <= 1024 * 1024 and 0 <= before <= size <= allocated):
            raise ValueError('Unexpected index bitmap metadata')
        value = ctypes.create_string_buffer(size)
        if lib.ntfs_attr_pread(attr, 0, size, value) != size:
            raise OSError(ctypes.get_errno(), 'Unable to read index bitmap')
        original = value.raw
        if before < size:
            if any(original[before:]):
                raise ValueError('Unexpected uninitialized bitmap contents')
            last = ctypes.create_string_buffer(original[-1:], 1)
            if lib.ntfs_attr_pwrite(attr, size - 1, 1, last) != 1:
                raise OSError(ctypes.get_errno(), 'Unable to initialize index bitmap')
        check = ctypes.create_string_buffer(size)
        if (info.data_size != size or info.initialized_size != size or
                lib.ntfs_attr_pread(attr, 0, size, check) != size or check.raw != original):
            raise OSError('Index bitmap verification failed')
        after = info.initialized_size
    except BaseException as exc:
        failure = exc
    finally:
        if attr:
            lib.ntfs_attr_close(attr)
        if inode and lib.ntfs_inode_close(inode) != 0 and failure is None:
            failure = OSError(ctypes.get_errno(), 'Unable to close NTFS directory')
        if lib.ntfs_umount(volume, 0) != 0 and failure is None:
            failure = OSError(ctypes.get_errno(), 'Unable to close NTFS image')
    if failure:
        raise failure
    return size, before, after
