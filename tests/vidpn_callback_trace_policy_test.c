/* Host-only exercise of the actual registry validator, no Windows/hive access. */
#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <assert.h>
typedef uint32_t DWORD;typedef unsigned char BYTE;typedef wchar_t WCHAR;typedef int HKEY;
#define A04_VIDPN_CALLBACK_TRACE 1
#define HKEY_LOCAL_MACHINE 0
#define REG_OPTION_OPEN_LINK 8
#define KEY_READ 1
#define REG_BINARY 3
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_NO_MORE_ITEMS 259
#define ERROR_INVALID_DATA 13
#define ERROR_RETRY 1237
#define lstrcmpW wcscmp
static DWORD epoch[108],record[108],record_type,record_bytes,children,reads,change_epoch;
static const WCHAR *label;
static DWORD RegOpenKeyExW(HKEY a,const WCHAR*b,DWORD c,DWORD d,HKEY*e){*e=1;return 0;}
static DWORD RegQueryValueExW(HKEY a,const WCHAR*b,void*c,DWORD*t,BYTE*out,DWORD*n){*t=REG_BINARY;*n=432;memcpy(out,epoch,432);if(change_epoch&&reads)((DWORD*)out)[6]++;reads++;return 0;}
static DWORD RegQueryInfoKeyW(HKEY a,void*b,void*c,void*d,DWORD*sub,void*f,void*g,void*h,void*i,void*j,void*k,void*l){*sub=children;return 0;}
static DWORD RegEnumValueW(HKEY a,DWORD i,WCHAR*name,DWORD*n,void*x,DWORD*t,BYTE*out,DWORD*b){if(i>1)return ERROR_NO_MORE_ITEMS;if(!i){wcscpy(name,L"EpochV1");*n=7;*t=REG_BINARY;*b=432;memcpy(out,epoch,432);}else{wcscpy(name,label);*n=wcslen(label);*t=record_type;*b=record_bytes;memcpy(out,record,record_bytes);}return 0;}
static void RegCloseKey(HKEY k){}
#include "../tools/winpe/vidpn-callback-trace-policy.h"
static void setup(void){memset(epoch,0,sizeof(epoch));epoch[0]=1;epoch[1]=432;epoch[6]=1;epoch[7]=1;epoch[10]=2;epoch[20]=7;epoch[21]=4;memcpy(record,epoch,432);record[2]=1;record[4]=1;record[20]=0;record[21]=0;label=L"C1S0";record_type=3;record_bytes=432;children=reads=change_epoch=0;}
int main(void){
 setup();assert(!vidpn_callback_trace());
 setup();record_bytes=4;assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 setup();record_type=1;assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 setup();label=L"C2U0";record[2]=2;assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 setup();label=L"C1F0";assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 setup();label=L"C1F0";record[5]=0xc0000001;assert(!vidpn_callback_trace());
 setup();record[6]=99;assert(!vidpn_callback_trace()); /* stale, not current-success */
 setup();children=1;assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 setup();change_epoch=1;assert(vidpn_callback_trace()==ERROR_RETRY);
 setup();epoch[1]=80;assert(vidpn_callback_trace()==ERROR_INVALID_DATA);
 return 0;
}
