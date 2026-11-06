#!/usr/bin/env python3
"""Compile the actual UART sender with simulated Win32 and decode its output."""
import importlib.util,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('decode',root/'tools/recovery/decode-audit-stream.py');decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
header=r'''
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <string.h>
typedef uint32_t DWORD; typedef wchar_t WCHAR; typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_SIZE 0xffffffffu
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0
#define ERROR_BUSY 170
#define FILE_DEVICE_UNKNOWN 34
#define METHOD_BUFFERED 0
#define FILE_WRITE_ACCESS 2
#define CTL_CODE(a,b,c,d) (((a)<<16)|((d)<<14)|((b)<<2)|(c))
#define CopyMemory memcpy
WCHAR *GetCommandLineW(void); DWORD GetTickCount(void); int DeviceIoControl(HANDLE,DWORD,void*,DWORD,void*,DWORD,DWORD*,void*); DWORD GetLastError(void); void Sleep(DWORD); HANDLE CreateFileW(const WCHAR*,DWORD,DWORD,void*,DWORD,DWORD,void*); DWORD GetFileSize(HANDLE,void*); void *HeapAlloc(void*,DWORD,size_t); void *GetProcessHeap(void); int ReadFile(HANDLE,void*,DWORD,DWORD*,void*); int CloseHandle(HANDLE); int HeapFree(void*,DWORD,void*); void ExitProcess(unsigned);
'''
harness=r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
static jmp_buf out;static unsigned exitcode,clock_ms,busy;static char data[20001];
WCHAR *GetCommandLineW(void){static WCHAR s[]=L"report.exe 0123456789ABCDEF";return s;}
DWORD GetTickCount(void){return clock_ms;}
int DeviceIoControl(HANDLE h,DWORD c,void*p,DWORD n,void*o,DWORD z,DWORD*g,void*v){(void)h;(void)c;(void)o;(void)z;(void)g;(void)v;if(!busy++){return 0;}busy=0;return fwrite(p,1,n,stdout)==n;}
DWORD GetLastError(void){return ERROR_BUSY;} void Sleep(DWORD n){clock_ms+=n;}
HANDLE CreateFileW(const WCHAR*p,DWORD a,DWORD b,void*c,DWORD d,DWORD e,void*f){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return (HANDLE)1;}
DWORD GetFileSize(HANDLE h,void*p){(void)h;(void)p;return sizeof(data)-1;}
void *HeapAlloc(void*h,DWORD f,size_t n){(void)h;(void)f;return malloc(n);}void *GetProcessHeap(void){return (HANDLE)1;}
int ReadFile(HANDLE h,void*p,DWORD n,DWORD*g,void*x){(void)h;(void)x;memcpy(p,data,n);*g=n;return 1;}
int CloseHandle(HANDLE h){(void)h;return 1;} int HeapFree(void*h,DWORD f,void*p){(void)h;(void)f;free(p);return 1;}
void ExitProcess(unsigned x){exitcode=x;longjmp(out,1);}
#include "SOURCE_FILE"
int main(void){unsigned i;for(i=0;i<20000;i++)data[i]=(char)(i%251+1);if(!setjmp(out))mainCRTStartup();return exitcode;}
'''.replace('SOURCE_FILE',str(root/'tools/winpe/uart-audit-stream.c'))
(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cpu-sender-',dir=root/'build') as t:
 d=Path(t);(d/'windows.h').write_text(header);(d/'test.c').write_text(harness);out=d/'test'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',str(d),str(d/'test.c'),'-o',str(out)],check=True)
 wire=subprocess.check_output([str(out)]);payload=decoder.decode(wire,'0123456789ABCDEF')
 assert payload==bytes(i%251+1 for i in range(20000))
 lines=wire.splitlines(keepends=True)
 for invalid in (b''.join(lines[:-2]+lines[-1:]), b''.join(lines[:2]+lines[3:4]+lines[2:3]+lines[4:]), b''.join(lines[:2]+lines[1:])):
  try:decoder.decode(invalid,'0123456789ABCDEF')
  except ValueError:continue
  raise AssertionError('bad sequence accepted')
print('Actual C UART sender: busy retries, framed bytes and receiver CRC verified')
