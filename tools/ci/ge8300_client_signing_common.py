from pathlib import Path
# SPDX-License-Identifier: MIT
# PE signing payload verifier retained from public f25a594e, source only.
import struct
def require(ok, message):
    if not ok:
        raise ValueError(message)


def decode_inf(data):
    if data.startswith(b'\xff\xfe'):
        return data[2:].decode('utf-16-le'), 'utf-16-le', b'\xff\xfe'
    if data.startswith(b'\xfe\xff'):
        return data[2:].decode('utf-16-be'), 'utf-16-be', b'\xfe\xff'
    if data.startswith(b'\xef\xbb\xbf'):
        return data[3:].decode('utf-8'), 'utf-8', b'\xef\xbb\xbf'
    return data.decode('utf-8'), 'utf-8', b''


def pe(data):
    require(len(data) >= 64 and data[:2] == b'MZ', 'PE DOS header missing')
    offset = struct.unpack_from('<I', data, 60)[0]
    require(offset + 24 + 136 <= len(data) and data[offset:offset+4] == b'PE\0\0', 'PE header missing')
    require(struct.unpack_from('<H', data, offset+4)[0] == 0x1c4, 'ARMNT required')
    optional = offset + 24
    require(struct.unpack_from('<H', data, optional)[0] == 0x10b, 'ARM32 PE32 required')
    directory = optional + 96 + 4*8
    certificate, size = struct.unpack_from('<II', data, directory)
    return dict(checksum=optional+64, security_directory=directory, certificate=certificate, certificate_bytes=size)


def unchanged_signed_payload(unsigned, signed):
    before, after = pe(unsigned), pe(signed)
    require(before['certificate_bytes'] == 0 and after['certificate_bytes'] > 0, 'Unsigned to signed PE transition required')
    require(after['checksum'] == before['checksum'] and after['security_directory'] == before['security_directory'], 'PE header moved')
    require(len(signed) > len(unsigned) and unsigned != signed, 'Signing must change file hash')
    c, n = after['certificate'], after['certificate_bytes']
    require(c >= len(unsigned) and c - len(unsigned) <= 7 and c % 8 == 0 and c+n <= len(signed), 'Unexpected certificate placement')
    a, b = bytearray(unsigned), bytearray(signed[:len(unsigned)])
    for start, length in ((before['checksum'], 4), (before['security_directory'], 8)):
        a[start:start+length] = b'\0' * length
        b[start:start+length] = b'\0' * length
    require(a == b and not any(signed[len(unsigned):c]) and not any(signed[c+n:]), 'Signing changed non-signature payload')



# Windows make dependency parser from the validated source4a path.
import re
def parse_make_dependencies(text):
 # Clang's Windows make output preserves path separators (C:\dir\file)
 # and escapes spaces as \ . POSIX shlex destroys those separators.
 text=re.sub(r'\\\r?\n',' ',text.lstrip('\ufeff'))
 if not text.startswith('ge8300-unit:'):raise ValueError('Unknown compiler dependency target')
 text=text[len('ge8300-unit:'):];result=[];token=[];i=0
 while i<len(text):
  c=text[i]
  if c.isspace():
   if token:result.append(''.join(token));token=[]
   i+=1;continue
  if c=='#':
   end=text.find('\n',i);i=len(text) if end<0 else end;continue
  if c=='\\' and i+1<len(text) and (text[i+1] in ' \t#'):
   token.append(text[i+1]);i+=2;continue
  if c=='$' and i+1<len(text) and text[i+1]=='$':token.append('$');i+=2;continue
  token.append(c);i+=1
 if token:result.append(''.join(token))
 return result

def dependencies(depfile):
 return parse_make_dependencies(Path(depfile).read_text(encoding='utf-8-sig'))
