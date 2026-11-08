#!/usr/bin/env python3
"""Run the actual install routine with unselected-node and failing Win32 boundaries."""
from pathlib import Path
import argparse,subprocess
ap=argparse.ArgumentParser();ap.add_argument('--scratch',type=Path,required=True);a=ap.parse_args();a.scratch.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];s=(r/'tools/winpe/peripheral-batch-reused.h').read_text();fn=s[s.index('static DWORD deferred_install('):s.index('\nstatic DWORD deferred_reboot_waiter(')]
shim=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint64_t ULONGLONG;typedef uint32_t DWORD;typedef uint8_t BYTE;typedef uint16_t WCHAR;typedef int BOOL;typedef void *HDEVINFO;
typedef struct{uint32_t Data1;uint16_t Data2,Data3;BYTE Data4[8];} GUID;
typedef struct{DWORD cbSize;GUID ClassGuid;} SP_DEVINFO_DATA;
typedef struct{DWORD dwLowDateTime,dwHighDateTime;} FILETIME;
typedef struct{DWORD cbSize,Flags,FlagsEx;WCHAR DriverPath[260];} SP_DEVINSTALL_PARAMS_W;
typedef struct{DWORD cbSize,DriverType;uint64_t DriverVersion;FILETIME DriverDate;WCHAR Description[20],MfgName[20],ProviderName[20];} SP_DRVINFO_DATA_W;
#define TRUE 1
#define FALSE 0
#define SPDIT_CLASSDRIVER 1
#define SPDIT_COMPATDRIVER 2
#define DI_ENUMSINGLEINF 1
#define DI_DONOTCALLCONFIGMG 2
#define DI_QUIETINSTALL 4
#define DI_NEEDREBOOT 8
#define DI_FLAGSEX_ALLOWEXCLUDEDDRVS 16
#define DIF_SELECTBESTCOMPATDRV 23
#define DIF_REGISTER_COINSTALLERS 34
#define DIF_INSTALLINTERFACES 32
#define DIF_INSTALLDEVICE 2
#define ERROR_NO_MORE_ITEMS 259
#define ERROR_DUP_NAME 52
#define ERROR_REVISION_MISMATCH 1306
#define ERROR_INVALID_DATA 13
#define ERROR_ACCESS_DENIED 5
#define A04_PERIPHERAL_FIRST_INSTALL_MASK 128
#define EV_PNP_API_BEGIN 30
static struct{uint64_t version,date;const WCHAR *cls;} targets[8];
#define deferred_pins targets
static DWORD deferred_current_target,deferred_current_step,deferred_failure_target,deferred_failure_step,log_error,last,fail_at,emitted_step,result,baseline_error;
static int deferred_reboot_needed,selected,dif_install_calls,destroyed,badguid,badversion,duplicate,lose_reboot,baseline_calls;
static const GUID hid={0x745a17a0,0x74d3,0x11d0,{0xb6,0xfe,0,0xa0,0xc9,0x0f,0x57,0xda}};
static DWORD GetLastError(void){return last;}
static int api(void){if(fail_at==deferred_current_step){last=ERROR_ACCESS_DENIED;return 0;}return 1;}
static void pnp_record(unsigned i,DWORD event,DWORD code){assert(i==7);if(event==0x106)emitted_step=code;if(event==0x105)result=code;}
static DWORD first_keypad_baseline(unsigned i,HDEVINFO h,SP_DEVINFO_DATA*d){(void)h;assert(i==7);assert(d->ClassGuid.Data1==0);baseline_calls++;return baseline_error;}
static int lstrcmpW(const WCHAR*a,const WCHAR*b){while(*a&&*a==*b){a++;b++;}return *a!=*b;}
static int lstrcmpiW(const WCHAR*a,const WCHAR*b){while(*a&&*b){WCHAR x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 1;}return *a!=*b;}
static void lstrcpyW(WCHAR*a,const WCHAR*b){while((*a++=*b++));}
static int SetupDiGetDeviceInstallParamsW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DEVINSTALL_PARAMS_W*p){(void)h;(void)d;if(!api())return 0;p->Flags=lose_reboot&&dif_install_calls?0:DI_NEEDREBOOT;return 1;}
static int SetupDiSetDeviceInstallParamsW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DEVINSTALL_PARAMS_W*p){(void)h;(void)d;assert(p->Flags&DI_DONOTCALLCONFIGMG);assert(p->Flags&DI_NEEDREBOOT);return api();}
static int SetupDiBuildDriverInfoList(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD type){(void)h;(void)d;assert(type==2);return api();}
static void driver(SP_DRVINFO_DATA_W*p){p->DriverType=2;p->DriverVersion=badversion?9:8;p->DriverDate.dwLowDateTime=10;p->Description[0]='K';p->Description[1]=0;p->MfgName[0]=p->ProviderName[0]=0;}
static int SetupDiEnumDriverInfoW(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD t,DWORD n,SP_DRVINFO_DATA_W*p){(void)h;(void)d;assert(t==2);if(!api())return 0;if(n){last=ERROR_NO_MORE_ITEMS;if(!duplicate)return 0;}driver(p);return 1;}
static int SetupDiCallClassInstaller(DWORD f,HDEVINFO h,SP_DEVINFO_DATA*d){(void)h;if(!api())return 0;if(f==DIF_SELECTBESTCOMPATDRV){selected=1;d->ClassGuid=hid;if(badguid)d->ClassGuid.Data1++;}else{assert(selected);if(f==DIF_INSTALLDEVICE)dif_install_calls++;}return 1;}
static int SetupDiGetSelectedDriverW(HDEVINFO h,SP_DEVINFO_DATA*d,SP_DRVINFO_DATA_W*p){(void)h;(void)d;assert(selected);if(!api())return 0;driver(p);return 1;}
static void SetupDiDestroyDriverInfoList(HDEVINFO h,SP_DEVINFO_DATA*d,DWORD type){(void)h;(void)d;assert(type==2);destroyed++;}
static void reset(void){last=fail_at=emitted_step=result=baseline_error=0;selected=dif_install_calls=destroyed=badguid=badversion=duplicate=lose_reboot=baseline_calls=deferred_reboot_needed=0;}
'''
main=r'''
int main(void){SP_DEVINFO_DATA d;unsigned step;targets[7].version=8;targets[7].date=10;targets[7].cls=(const WCHAR*)L"{745a17a0-74d3-11d0-b6fe-00a0c90f57da}";
reset();memset(&d,0,sizeof(d));assert(!deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf"));assert(selected&&dif_install_calls==1&&destroyed==1&&deferred_reboot_needed&&baseline_calls==1&&emitted_step==19);
/* Every external boundary retains its exact error and failure step. No live restart APIs exist in this fixture. */
for(step=2;step<=18;step++){if(step==7||step==10||step==11||step==16)continue;reset();memset(&d,0,sizeof(d));fail_at=step;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_ACCESS_DENIED);assert(emitted_step==step&&deferred_failure_step==step);assert(dif_install_calls==(step==18));}
reset();memset(&d,0,sizeof(d));baseline_error=ERROR_REVISION_MISMATCH;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_REVISION_MISMATCH&&emitted_step==1&&!selected);
reset();memset(&d,0,sizeof(d));badversion=1;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_REVISION_MISMATCH&&emitted_step==7&&!selected);
reset();memset(&d,0,sizeof(d));duplicate=1;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_DUP_NAME&&emitted_step==6&&!selected);
reset();memset(&d,0,sizeof(d));badguid=1;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_REVISION_MISMATCH&&emitted_step==11&&!dif_install_calls);
reset();memset(&d,0,sizeof(d));lose_reboot=1;assert(deferred_install(7,0,&d,(const WCHAR*)L"keypad.inf")==ERROR_INVALID_DATA&&emitted_step==19&&dif_install_calls==1);
return 0;}
'''
f=a.scratch/'first-install.c';f.write_text(shim+fn+main);exe=a.scratch/'first-install'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fshort-wchar','-fsanitize=address,undefined',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('ACTUAL_INSTALL_ROUTINE_UNBOUND_SELECTION_STRICT_CLASS_API_FAILURES_PASS')
