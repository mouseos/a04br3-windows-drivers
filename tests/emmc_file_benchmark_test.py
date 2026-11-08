#!/usr/bin/env python3
"""Run the real client with in-memory Win32 mocks; no speed/hardware claims."""
import argparse,json,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--out',required=True);o=Path(p.parse_args().out).resolve();assert r.parent.parent in o.parents;o.mkdir(parents=True,exist_ok=True)
h=r'''
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
typedef uint32_t DWORD;typedef unsigned char BYTE;typedef int BOOL;typedef wchar_t WCHAR;typedef void *HANDLE;
typedef union {uint64_t QuadPart;} ULARGE_INTEGER;typedef union {int64_t QuadPart;} LARGE_INTEGER;
typedef struct {DWORD dwFileAttributes;} BY_HANDLE_FILE_INFORMATION;
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define NO_ERROR 0
#define ERROR_BAD_PATHNAME 161
#define ERROR_BAD_DEVICE 1200
#define ERROR_WRONG_DISK 34
#define ERROR_NOT_SUPPORTED 50
#define ERROR_DISK_FULL 112
#define ERROR_WRITE_FAULT 29
#define ERROR_READ_FAULT 30
#define ERROR_CRC 23
#define ERROR_TIMEOUT 1460
#define ERROR_INVALID_PARAMETER 87
#define DRIVE_FIXED 3
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_REPARSE_POINT 1024
#define FILE_ATTRIBUTE_TEMPORARY 256
#define FILE_READ_ONLY_VOLUME 0x80000
#define FILE_READ_ATTRIBUTES 128
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define CREATE_NEW 1
#define FILE_FLAG_BACKUP_SEMANTICS 0x2000000
#define FILE_FLAG_OPEN_REPARSE_POINT 0x200000
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
#define FILE_FLAG_DELETE_ON_CLOSE 0x4000000
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define DELETE 0x10000
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 4
#define FILE_BEGIN 0
#define STD_OUTPUT_HANDLE ((DWORD)-11)
DWORD GetLastError(void);DWORD GetFileAttributesW(const WCHAR*);DWORD GetDriveTypeW(const WCHAR*);
BOOL GetVolumeInformationW(const WCHAR*,WCHAR*,DWORD,DWORD*,DWORD*,DWORD*,WCHAR*,DWORD);
int lstrcmpW(const WCHAR*,const WCHAR*);int lstrcmpiW(const WCHAR*,const WCHAR*);
BOOL GetDiskFreeSpaceW(const WCHAR*,DWORD*,DWORD*,DWORD*,DWORD*);BOOL GetDiskFreeSpaceExW(const WCHAR*,ULARGE_INTEGER*,void*,void*);
BOOL CreateDirectoryW(const WCHAR*,void*);HANDLE CreateFileW(const WCHAR*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
BOOL GetFileInformationByHandle(HANDLE,BY_HANDLE_FILE_INFORMATION*);void*VirtualAlloc(void*,size_t,DWORD,DWORD);
BOOL VirtualFree(void*,size_t,DWORD);BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
BOOL FlushFileBuffers(HANDLE);BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD);DWORD GetTickCount(void);
BOOL CloseHandle(HANDLE);BOOL RemoveDirectoryW(const WCHAR*);WCHAR*GetCommandLineW(void);HANDLE GetStdHandle(DWORD);void ExitProcess(DWORD);
'''
(o/'windows.h').write_text(h)
s='#include <assert.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdio.h>\n#include "'+str(r/'tools/winpe/mt8167-emmc-file-benchmark.c')+'"\n'+r'''
static BYTE storage[FILE_BYTES];static char logs[4096];static unsigned loglen,pos,writes,reads,flushes,deletes,removes,opens;
static int scenario,owned;static DWORD err,ticks;
DWORD GetLastError(void){return err;}
DWORD GetFileAttributesW(const WCHAR*p){assert(!wcscmp(p,L"C:\\"));return FILE_ATTRIBUTE_DIRECTORY;}
DWORD GetDriveTypeW(const WCHAR*p){(void)p;return DRIVE_FIXED;}
BOOL GetVolumeInformationW(const WCHAR*p,WCHAR*l,DWORD a,DWORD*b,DWORD*c,DWORD*f,WCHAR*fs,DWORD z){(void)p;(void)a;(void)b;(void)c;(void)z;wcscpy(l,L"Windows");wcscpy(fs,L"NTFS");*f=0;return 1;}
int lstrcmpW(const WCHAR*a,const WCHAR*b){return wcscmp(a,b);}int lstrcmpiW(const WCHAR*a,const WCHAR*b){return wcscmp(a,b);}
BOOL GetDiskFreeSpaceW(const WCHAR*p,DWORD*a,DWORD*b,DWORD*c,DWORD*d){(void)p;*a=8;*b=512;*c=*d=100000;return 1;}
BOOL GetDiskFreeSpaceExW(const WCHAR*p,ULARGE_INTEGER*a,void*b,void*c){(void)p;(void)b;(void)c;a->QuadPart=1u<<30;return 1;}
BOOL CreateDirectoryW(const WCHAR*p,void*x){(void)x;assert(!wcscmp(p,test_directory));if(scenario==1){err=183;return 0;}return 1;}
HANDLE CreateFileW(const WCHAR*p,DWORD a,DWORD share,void*x,DWORD how,DWORD flags,HANDLE t){(void)a;(void)x;(void)t;
 if(!wcscmp(p,test_directory)){assert(how==OPEN_EXISTING);return (HANDLE)3;}
 assert(!wcscmp(p,test_file)&&how==CREATE_NEW&&share==0);opens++;
 assert((flags&(FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_DELETE_ON_CLOSE))==(FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_DELETE_ON_CLOSE));
 if(scenario==2){err=80;return INVALID_HANDLE_VALUE;}owned=1;return (HANDLE)4;
}
BOOL GetFileInformationByHandle(HANDLE h,BY_HANDLE_FILE_INFORMATION*i){assert(h==(HANDLE)3);i->dwFileAttributes=scenario==3?FILE_ATTRIBUTE_REPARSE_POINT:FILE_ATTRIBUTE_DIRECTORY;return 1;}
void*VirtualAlloc(void*p,size_t n,DWORD a,DWORD b){(void)p;(void)a;(void)b;return aligned_alloc(4096,n);}
BOOL VirtualFree(void*p,size_t n,DWORD f){(void)n;(void)f;free(p);return 1;}
BOOL WriteFile(HANDLE h,const void*p,DWORD n,DWORD*d,void*x){(void)x;
 if(h==(HANDLE)2){assert(loglen+n<sizeof(logs));memcpy(logs+loglen,p,n);loglen+=n;logs[loglen]=0;*d=n;return 1;}
 assert(h==(HANDLE)4&&owned&&n==4096&&pos+n<=FILE_BYTES&&!((uintptr_t)p&4095));
 if(scenario==4&&writes==1){err=29;return 0;}memcpy(storage+pos,p,n);pos+=n;writes++;*d=n;return 1;
}
BOOL ReadFile(HANDLE h,void*p,DWORD n,DWORD*d,void*x){(void)x;assert(h==(HANDLE)4&&owned&&n==4096&&pos+n<=FILE_BYTES&&flushes==1);memcpy(p,storage+pos,n);if(scenario==6)((BYTE*)p)[0]^=1;pos+=n;reads++;*d=n;return 1;}
BOOL FlushFileBuffers(HANDLE h){assert(h==(HANDLE)4);flushes++;if(scenario==5){err=29;return 0;}return 1;}
BOOL SetFilePointerEx(HANDLE h,LARGE_INTEGER x,LARGE_INTEGER*y,DWORD m){(void)y;assert(h==(HANDLE)4&&x.QuadPart==0&&m==FILE_BEGIN);pos=0;return 1;}
DWORD GetTickCount(void){ticks+=scenario==7?PHASE_LIMIT_MS:1;return ticks;}
BOOL CloseHandle(HANDLE h){if(h==(HANDLE)4){assert(owned);owned=0;deletes++;}else assert(h==(HANDLE)3);return 1;}
BOOL RemoveDirectoryW(const WCHAR*p){assert(!wcscmp(p,test_directory));removes++;if(scenario==2){err=145;return 0;}return 1;}
WCHAR*GetCommandLineW(void){static WCHAR c[]=L"client --bench";return c;}HANDLE GetStdHandle(DWORD d){(void)d;return (HANDLE)2;}void ExitProcess(DWORD d){(void)d;abort();}
int main(void){for(scenario=0;scenario<8;scenario++){
 loglen=pos=writes=reads=flushes=deletes=removes=opens=0;owned=0;err=0;ticks=0xfffffff0u;output=(HANDLE)2;output_ok=1;
 DWORD result=verify();assert(!owned);
 if(!scenario){assert(!result&&writes==1024&&reads==1024&&flushes==1&&deletes==1&&removes==1);assert(strstr(logs,"WRITE_BYTES=0x00400000")&&strstr(logs,"READ_BYTES=0x00400000")&&strstr(logs,"FLUSH_OK=0x00000001")&&strstr(logs,"RESULT=PASS"));}
 else {assert(result&&strstr(logs,"RESULT=FAIL"));if(scenario==1)assert(!opens&&!deletes&&!removes);if(scenario==2)assert(opens==1&&!deletes);if(scenario==3)assert(!opens&&!deletes);if(scenario>=4)assert(deletes==1&&removes==1);if(scenario==5)assert(!reads);if(scenario==7)assert(!writes&&!reads);}
 }
 puts("{\"scenarios\":8,\"new_file_only\":true,\"max_payload_bytes\":4194304,\"flush_before_read\":true,\"cleanup_verified\":true,\"tick_wrap_checked\":true,\"hardware_speed_measured\":false}");}
'''
(o/'test.c').write_text(s);env=dict(os.environ,TMPDIR=str(o))
subprocess.run(['cc','-std=c11','-fno-builtin','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(o),str(o/'test.c'),'-o',str(o/'test')],env=env,check=True)
result=subprocess.check_output([str(o/'test')],text=True,env=env);(o/'result.json').write_text(json.dumps(json.loads(result),indent=2)+'\n');print(result,end='')
