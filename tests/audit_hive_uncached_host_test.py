#!/usr/bin/env python3
"""Exercise actual Win32 helper against historical bytes using simulated I/O."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1];fixture=root.parents[1]/'port/gsi/logs/ios-feasibility/windows-rt-watchdog-audit-20261105/SYSTEM'
harness=r'''
#define __declspec(x)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "SOURCE"
static unsigned char *source,*copy;static size_t len,sa,da;static unsigned opens,mode,code;static jmp_buf done;
WCHAR *GetCommandLineW(void){static WCHAR s[]={'t','e','s','t',' ','S','Y','S','T','E','M',0};return s;}
HANDLE CreateFileW(const WCHAR *p,DWORD access,DWORD share,void*s,DWORD open,DWORD flags,HANDLE t){(void)access;(void)share;(void)s;(void)open;(void)t;if(p[0]=='C'){if(!(flags&NO_BUFFERING))abort();sa=0;opens++;return (HANDLE)1;}da=0;return (HANDLE)2;}
BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER*s){(void)h;s->value=len;return 1;}
BOOL ReadFile(HANDLE h,void*b,DWORD n,DWORD*g,void*x){(void)x;size_t *pos=h==(HANDLE)1?&sa:&da;unsigned char *s=h==(HANDLE)1?source:copy;if(*pos+n>len)return 0;memcpy(b,s+*pos,n);if(mode==1&&h==(HANDLE)2&&*pos==0)((unsigned char*)b)[123]^=1;if(mode==2&&h==(HANDLE)1&&opens==2&&*pos==0)((unsigned char*)b)[123]^=1;*pos+=n;*g=n;return 1;}
BOOL CloseHandle(HANDLE h){(void)h;return 1;}void *VirtualAlloc(void*a,size_t n,DWORD x,DWORD y){(void)a;(void)x;(void)y;return malloc(n);}BOOL VirtualFree(void*p,size_t n,DWORD f){(void)n;(void)f;free(p);return 1;}
DWORD GetLastError(void){return 0;}HANDLE GetStdHandle(DWORD n){(void)n;return (HANDLE)3;}BOOL WriteFile(HANDLE h,const void*p,DWORD n,DWORD*w,void*x){(void)h;(void)x;*w=n;return fwrite(p,1,n,stdout)==n;}
void ExitProcess(DWORD x){code=x;longjmp(done,1);}
int main(int argc,char **argv){if(argc!=3)return 10;mode=(unsigned)atoi(argv[2]);FILE*f=fopen(argv[1],"rb");if(!f)return 11;fseek(f,0,SEEK_END);len=(size_t)ftell(f);rewind(f);source=malloc(len);copy=malloc(len);if(!source||!copy||fread(source,1,len,f)!=len)return 12;fclose(f);memcpy(copy,source,len);if(mode==3){source[0x100]^=1;copy[0x100]^=1;}if(mode==4)len--;if(!setjmp(done))mainCRTStartup();free(source);free(copy);return code;}
'''.replace('SOURCE',str(root/'tools/winpe/audit-hive-uncached.c'))
(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='hive-uncached-',dir=root/'build/tmp') as t:
 p=Path(t);(p/'harness.c').write_text(harness);exe=p/'test'
 subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(p/'harness.c'),'-o',str(exe)],check=True)
 for mode in range(5):
  result=subprocess.run([str(exe),str(fixture),str(mode)],capture_output=True)
  assert (result.returncode==0)==(mode==0),(mode,result.stdout,result.stderr)
  if mode==0:assert b'A04_HIVE_SEQUENCE_PRIMARY=00000324' in result.stdout and b'A04_HIVE_SEQUENCE_SECONDARY=00000323' in result.stdout and b'A04_HIVE_UNCACHED_TWO_PASS_MATCH' in result.stdout
 for suffix in ('.LOG1','.LOG2'):
  result=subprocess.run([str(exe),str(fixture)+suffix,'0'],capture_output=True)
  assert result.returncode==0 and b'A04_HIVE_UNCACHED_TWO_PASS_MATCH' in result.stdout,(suffix,result.stdout,result.stderr)
print('actual helper: real dirty hive, uncached flag, copy/source corruption, invalid header and alignment tested')
