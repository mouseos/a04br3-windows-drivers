#!/usr/bin/env python3
"""Exercise the actual parser against Win32 GetCommandLineW-style strings."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'tools/winpe/observe-msdc-storage.c').read_text();a=s.index('static int command_mode(');b=s.index('static int file_pin(',a)
harness=r'''
#include <wchar.h>
#include <stdio.h>
typedef wchar_t WCHAR;
#define FALSE 0
#define CSTR_EQUAL 2
static int CompareStringOrdinal(const WCHAR*a,int al,const WCHAR*b,int bl,int ignore){(void)ignore;return al==bl&&!wcsncmp(a,b,al)?2:1;}
PARSER
int main(void){struct {const WCHAR*s;int want;} tests[]={
 {L"X:\\a04br3\\observe.exe",0},
 {L"X:\\a04br3\\observe.exe   ",0},
 {L"X:\\a04br3\\observe.exe --verify-package",1},
 {L"X:\\a04br3\\observe.exe --verify-package   ",1},
 {L"\"X:\\path with spaces\\observe.exe\"\t--verify-package \t",1},
 {L"\"X:\\observe.exe\" --wait  ",2},
 {L"\"X:\\observe.exe\" \"--verify-package\"  ",1},
 {L"\"X:\\observe.exe\" \"--wait\"\t",2},
 {L"X:\\observe.exe --verify-package --wait",-1},
 {L"X:\\observe.exe --other",-1},
 {L"\"X:\\unclosed --wait",-1},
 {L"\"X:\\observe.exe\"--wait",-1},
 };
 unsigned i;for(i=0;i<sizeof(tests)/sizeof(tests[0]);i++){int v=command_mode(tests[i].s);if(v!=tests[i].want){fprintf(stderr,"case %u got %d want %d\n",i,v,tests[i].want);return 1;}}puts("MSDC command line parser: PASS");return 0;}
'''.replace('PARSER',s[a:b])
(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='msdc-args-',dir=root/'build') as t:
 p=Path(t);(p/'test.c').write_text(harness);exe=p/'test';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(p/'test.c'),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
