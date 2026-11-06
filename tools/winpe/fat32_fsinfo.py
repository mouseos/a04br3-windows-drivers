"""Prepare a new FAT32 image copy for file changes without fsck repair."""
import struct

def mark_free_space_unknown(path):
    with path.open('r+b') as image:
        boot = image.read(512)
        if len(boot) != 512:
            raise ValueError('short FAT32 boot sector')
        bps = struct.unpack_from('<H', boot, 11)[0]
        reserved = struct.unpack_from('<H', boot, 14)[0]
        fsinfo = struct.unpack_from('<H', boot, 48)[0]
        backup = struct.unpack_from('<H', boot, 50)[0]
        if bps != 512 or not 0 < fsinfo < reserved or boot[510:512] != b'\x55\xaa':
            raise ValueError('unsupported FAT32 FSInfo geometry')
        sectors = {fsinfo, backup + fsinfo}
        # Validate both first, before writing either one.
        for sector in sectors:
            if sector >= reserved:
                raise ValueError('invalid backup FSInfo location')
            image.seek(sector * bps)
            info = image.read(bps)
            if len(info) != bps or info[:4] != b'RRaA' or info[484:488] != b'rrAa' or info[508:512] != b'\x00\x00\x55\xaa':
                raise ValueError('invalid FAT32 FSInfo signature')
        for sector in sectors:
            image.seek(sector * bps + 488)
            image.write(struct.pack('<II', 0xffffffff, 0xffffffff))
