#!/usr/bin/env python3
"""実version/date readerをWin32境界stubで実行。端末/hiveアクセス無し。"""
import argparse,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args();a.scratch.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];h=(r/'tools/winpe/peripheral-batch-reused.h').read_text();c=(r/'tools/winpe/mt8167-peripheral-update.c').read_text()
functions=h[h.index('static DWORD deferred_driver_value('):h.index('static DWORD deferred_install(')]
functions+=c[c.index('static DWORD selected('):c.index('static DWORD installed_sys(')]
shim=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
typedef uint32_t DWORD,DEVPROPTYPE;typedef uint64_t ULONGLONG;typedef uint8_t BYTE;typedef wchar_t WCHAR;typedef void *HDEVINFO,*HKEY;typedef struct {DWORD a;} SP_DEVINFO_DATA;typedef struct {DWORD dwLowDateTime,dwHighDateTime;} FILETIME;
typedef struct {struct{DWORD a;uint16_t b,c;BYTE d[8];} guid;DWORD pid;} DEVPROPKEY;
#define INVALID_HANDLE_VALUE ((void*)-1)
#define DICS_FLAG_GLOBAL 1
#define DIREG_DRV 1
#define KEY_QUERY_VALUE 1
#define ERROR_NOT_FOUND 1168
#define ERROR_ACCESS_DENIED 5
#define ERROR_REVISION_MISMATCH 1306
#define ERROR_INVALID_DATA 13
#define REG_SZ 1
#define REG_BINARY 3
#define DEVPROP_TYPE_STRING 18
#define DEVPROP_TYPE_FILETIME 16
#define SPDRP_SERVICE 4
#define SPDRP_CLASSGUID 8
static struct{ULONGLONG version,base_version,date,base_date;const WCHAR *service,*cls;} targets[1];
static DWORD err,prop_error,property_type=DEVPROP_TYPE_STRING,registry_type=REG_SZ,calls;static int bad_version,bad_date;static DWORD forced_bytes;
static void record(unsigned a,unsigned b,DWORD c){(void)a;(void)b;(void)c;}
static DWORD GetLastError(void){return err;}
static DWORD copy_version(BYTE*out){const WCHAR*v=bad_version?L"65536.0.0.1":L"2026.11.8.1010";unsigned n=0;do{((WCHAR*)out)[n]=v[n];}while(v[n++]);return n*sizeof(WCHAR);}
static int SetupDiGetDevicePropertyW(HDEVINFO s,SP_DEVINFO_DATA*d,const DEVPROPKEY*k,DEVPROPTYPE*t,BYTE*v,DWORD cap,DWORD*n,DWORD f){(void)s;(void)d;(void)cap;(void)f;if(prop_error){err=prop_error;return 0;}if(k->pid==3){*t=property_type;*n=copy_version(v);}else{*t=DEVPROP_TYPE_FILETIME;*(uint64_t*)v=bad_date?8:7;*n=8;}if(forced_bytes)*n=forced_bytes;return 1;}
static HKEY SetupDiOpenDevRegKey(HDEVINFO s,SP_DEVINFO_DATA*d,DWORD a,DWORD b,DWORD c,DWORD f){(void)s;(void)d;(void)a;(void)b;(void)c;(void)f;++calls;return (void*)1;}
static DWORD RegQueryValueExW(HKEY k,const WCHAR*name,void*z,DWORD*t,BYTE*v,DWORD*n){(void)k;(void)z;if(name[6]=='V'){*t=registry_type;*n=copy_version(v);}else{*t=REG_BINARY;*(uint64_t*)v=bad_date?8:7;*n=8;}if(forced_bytes)*n=forced_bytes;return 0;}
static void RegCloseKey(HKEY k){(void)k;}
static DWORD property(HDEVINFO s,SP_DEVINFO_DATA*d,DWORD p,const WCHAR*w){(void)s;(void)d;(void)p;(void)w;return 0;}
static DWORD installed_inf(unsigned i,HDEVINFO s,SP_DEVINFO_DATA*d,int c){(void)i;(void)s;(void)d;(void)c;return 0;}
'''
main=r'''
int main(void){SP_DEVINFO_DATA d={0};targets[0].version=targets[0].base_version=((ULONGLONG)2026<<48)|((ULONGLONG)11<<32)|((ULONGLONG)8<<16)|1010;targets[0].date=targets[0].base_date=7;
assert(!selected(0,0,&d,1)&&calls==0);prop_error=ERROR_NOT_FOUND;assert(!selected(0,0,&d,1)&&calls==2);
prop_error=ERROR_ACCESS_DENIED;calls=0;assert(selected(0,0,&d,1)==ERROR_ACCESS_DENIED&&calls==0);
prop_error=0;property_type=REG_SZ;assert(selected(0,0,&d,1)==ERROR_INVALID_DATA&&calls==0);property_type=DEVPROP_TYPE_STRING;
bad_version=1;assert(selected(0,0,&d,1)==ERROR_INVALID_DATA&&calls==0);bad_version=0;
bad_date=1;assert(selected(0,0,&d,1)==ERROR_REVISION_MISMATCH&&calls==0);bad_date=0;
prop_error=ERROR_NOT_FOUND;registry_type=REG_BINARY;assert(selected(0,0,&d,1)==ERROR_INVALID_DATA);registry_type=REG_SZ;
forced_bytes=3;assert(selected(0,0,&d,1)==ERROR_INVALID_DATA);forced_bytes=0;
assert(!selected(0,0,&d,0));return 0;}
'''
with tempfile.TemporaryDirectory(dir=a.scratch,prefix='metadata-') as td:
 d=Path(td);f=d/'test.c';f.write_text(shim+functions+main);exe=d/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fshort-wchar','-fsanitize=address,undefined',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('EXACT_DEVNODE_MISSING_PROPERTY_ONLY_FALLBACK_STRICT_TYPES_BYTES_VALUES_PASS')
