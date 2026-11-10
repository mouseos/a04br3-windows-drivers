#!/usr/bin/env python3
import argparse,sys,pathlib
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools/ci'))
from ge8300_compiler_cache import parse_make_dependencies
p=argparse.ArgumentParser();p.add_argument('--original-trace',type=pathlib.Path);a=p.parse_args()
assert parse_make_dependencies('ge8300-unit: /a/b.c /space\\ name/header.h\n')==['/a/b.c','/space name/header.h']
assert parse_make_dependencies('ge8300-unit: C:\\src\\u.c \\\r\n C:\\Program\\ Files\\SDK\\x.h\r\n')==['C:\\src\\u.c','C:\\Program Files\\SDK\\x.h']
assert parse_make_dependencies('ge8300-unit: C:/a/u.c C:/cash$$/x.h escaped\\#name.h # comment\n')==['C:/a/u.c','C:/cash$/x.h','escaped#name.h']
assert parse_make_dependencies('ge8300-unit: \\\\server\\share\\x.h')==['\\\\server\\share\\x.h']
if a.original_trace:
 data=a.original_trace.read_bytes();paths=parse_make_dependencies(data.decode('utf-8-sig'));assert paths[0].endswith('\\third_party\\mesa-pco\\generated\\src\\imagination\\pco\\pco_info.c');assert any('Program Files (x86)' in x for x in paths);assert all(x.startswith('C:\\') for x in paths);print('PASS original Windows clang trace:',len(paths),'paths, source/backslashes/spaces preserved; raw trace unchanged')
 assert a.original_trace.read_bytes()==data
print('PASS make dependency Windows/UNC/POSIX/escaped-space/hash/dollar/CRLF contracts')
