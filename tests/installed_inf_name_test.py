#!/usr/bin/env python3
"""Execute the installed-only locator helper with bounded SetupAPI contracts."""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser();p.add_argument('--scratch',type=Path,required=True);a=p.parse_args();a.scratch.mkdir(parents=True,exist_ok=True)
s=(Path(__file__).resolve().parents[1]/'tools/winpe/mt8167-peripheral-update.c').read_text();start=s.index('static DWORD installed_inf_name(');fn=s[start:s.index('static DWORD installed_inf(unsigned i,',start)]
shim=r'''
#include <assert.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
typedef unsigned DWORD;typedef uint64_t ULONGLONG;typedef wchar_t WCHAR;typedef void* HDEVINFO;
typedef struct{DWORD dwHighDateTime,dwLowDateTime;} FT;
typedef struct{DWORD cbSize;} SP_DEVINFO_DATA;
typedef struct{DWORD cbSize,FlagsEx;} SP_DEVINSTALL_PARAMS_W;
typedef struct{DWORD cbSize;ULONGLONG DriverVersion;FT DriverDate;} SP_DRVINFO_DATA_W;
typedef struct{DWORD cbSize;WCHAR InfFileName[260];} SP_DRVINFO_DETAIL_DATA_W;
#define MAX_DEVICE_ID_LEN 200
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((void*)-1)
#define DI_FLAGSEX_INSTALLEDDRIVER 0x4000000
#define SPDIT_COMPATDRIVER 2
#define ERROR_INVALID_DATA 13
#define ERROR_NO_MORE_ITEMS 259
#define ERROR_REVISION_MISMATCH 1306
#define ERROR_INSUFFICIENT_BUFFER 122
#define ZeroMemory(p,n) memset(p,0,n)
#define lstrcpyW wcscpy
static struct{ULONGLONG version,date;} targets[]={{327,42}};
static DWORD last,mode,lists,sets,details;
static DWORD GetLastError(void){return last;}
static int SetupDiGetDeviceInstanceIdW(HDEVINFO h,SP_DEVINFO_DATA*d,WCHAR*p,DWORD n,DWORD*r){(void)h;(void)d;(void)r;assert(n==200);wcscpy(p,L"MT8167\\RENDER\\0");return 1;}
static HDEVINFO SetupDiCreateDeviceInfoList(void*a,void*b){assert(!a&&!b);return (void*)2;}
static int SetupDiOpenDeviceInfoW(HDEVINFO h,WCHAR*p,void*a,DWORD f,SP_DEVINFO_DATA*d){assert(h==(void*)2&&!a&&!f&&d->cbSize==sizeof(*d)&&!wcscmp(p,L"MT8167\\RENDER\\0"));return 1;}
static int SetupDiGetDeviceInstallParamsW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DEVINSTALL_PARAMS_W*p){(void)h;(void)d;assert(p->cbSize==sizeof(*p));return 1;}
static int SetupDiSetDeviceInstallParamsW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DEVINSTALL_PARAMS_W*p){(void)h;(void)d;assert(p->FlagsEx==DI_FLAGSEX_INSTALLEDDRIVER);return 1;}
static int SetupDiBuildDriverInfoList(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD t){(void)h;(void)d;assert(t==2);if(mode==1){last=5;return 0;}return 1;}
static int SetupDiEnumDriverInfoW(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD t,DWORD n,SP_DRVINFO_DATA_W*p){(void)h;(void)d;assert(t==2&&p->cbSize==sizeof(*p));if(n==1){if(mode==2)return 1;last=mode==3?5:259;return 0;}p->DriverVersion=mode==4?328:327;p->DriverDate.dwLowDateTime=42;return 1;}
static int SetupDiGetDriverInfoDetailW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DRVINFO_DATA_W*p,SP_DRVINFO_DETAIL_DATA_W*q,DWORD n,DWORD*r){(void)h;(void)d;(void)p;assert(n==sizeof(*q)&&q->cbSize==sizeof(*q));details++;wcscpy(q->InfFileName,L"C:\\Windows\\INF\\oem150.inf");*r=n+100;if(mode==5){last=5;return 0;}if(mode==6){for(unsigned i=0;i<260;i++)q->InfFileName[i]='x';}if(mode==7){last=122;return 0;}return 1;}
static int SetupDiDestroyDriverInfoList(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD t){(void)h;(void)d;assert(t==2);lists++;return 1;}
static int SetupDiDestroyDeviceInfoList(HDEVINFO h){assert(h==(void*)2);sets++;return 1;}
'''
main=r'''
int main(void){DWORD expected[]={0,5,13,5,1306,5,13,0};SP_DEVINFO_DATA d={sizeof(d)};WCHAR out[260];for(mode=0;mode<8;mode++){lists=sets=details=0;assert(installed_inf_name(0,(void*)1,&d,out)==expected[mode]);assert(sets==1&&lists==(mode!=1));assert(details==(mode==0||mode>=5));if(!expected[mode])assert(!wcscmp(out,L"C:\\Windows\\INF\\oem150.inf"));}return 0;}
'''
locator=s[s.index('static DWORD inf_location_missing('):s.index('/* Opt-in only',s.index('static DWORD inf_location_missing('))];shim+='\n#define ERROR_FILE_NOT_FOUND 2\n#define ERROR_NOT_FOUND 1168\nstatic DWORD selected_inf_probe_stage,selected_inf_locator_unverified;\n';f=a.scratch/'installed-inf-name.c';f.write_text(shim+locator+fn+main);exe=a.scratch/'installed-inf-name';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True);print('INSTALLED_ONLY_IDENTITY_BOUNDS_CLEANUP_AND_PARTIAL_STATIC_FIELDS_PASS')
