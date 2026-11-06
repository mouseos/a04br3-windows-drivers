/* Actual mapper code with a simulated volume namespace, never a real disk. */
#include <stdio.h>
#include <setjmp.h>
#include <wchar.h>
#include <stdlib.h>
#include "windows.h"
#include "winioctl.h"
static jmp_buf done;static unsigned scenario,code,indexn,assigned,drives_calls;static DWORD error;
HANDLE GetStdHandle(DWORD n){(void)n;return (HANDLE)1;}
BOOL WriteFile(HANDLE h,const void*p,DWORD n,DWORD*w,void*o){(void)h;(void)p;(void)o;*w=n;return 1;}
int lstrlenW(const WCHAR*s){return (int)wcslen(s);} WCHAR*lstrcpyW(WCHAR*a,const WCHAR*b){return wcscpy(a,b);} WCHAR*lstrcatW(WCHAR*a,const WCHAR*b){return wcscat(a,b);} int lstrcmpiW(const WCHAR*a,const WCHAR*b){return wcscmp(a,b);}
UINT GetWindowsDirectoryW(WCHAR*p,UINT n){(void)n;wcscpy(p,scenario==12?L"C:\\Windows":L"X:\\Windows");return 10;}
LONG RegOpenKeyExW(HKEY h,const WCHAR*p,DWORD a,DWORD b,HKEY*k){(void)h;(void)p;(void)a;(void)b;*k=(HKEY)3;return 0;} LONG RegCloseKey(HKEY h){(void)h;return 0;}
DWORD GetLogicalDrives(void){drives_calls++;if(scenario==1)return 4;if(scenario==9&&drives_calls>1)return 4;return 16;}
BOOL GetVolumeNameForVolumeMountPointW(const WCHAR*p,WCHAR*out,DWORD n){(void)p;(void)n;wcscpy(out,L"\\\\?\\RAM\\");return scenario!=10;}
HANDLE FindFirstVolumeW(WCHAR*p,DWORD n){(void)n;indexn=0;wcscpy(p,L"\\\\?\\RAM\\");return scenario==11?INVALID_HANDLE_VALUE:(HANDLE)2;}
BOOL FindNextVolumeW(HANDLE h,WCHAR*p,DWORD n){(void)h;(void)n;indexn++;if(scenario==6&&indexn==2){error=5;return 0;}if(indexn<=2){wcscpy(p,indexn==1?L"\\\\?\\VOL1\\":L"\\\\?\\VOL2\\");return 1;}error=ERROR_NO_MORE_FILES;return 0;}
BOOL FindVolumeClose(HANDLE h){(void)h;return 1;}
HANDLE CreateFileW(const WCHAR*p,DWORD a,DWORD b,void*c,DWORD d,DWORD e,HANDLE f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;if(scenario==8)return INVALID_HANDLE_VALUE;return (HANDLE)(intptr_t)(wcschr(p,L'1')?3:4);}
BOOL DeviceIoControl(HANDLE h,DWORD c,void*a,DWORD b,void*out,DWORD os,DWORD*n,void*x){VOLUME_DISK_EXTENTS*e=out;(void)c;(void)a;(void)b;(void)os;(void)x;*n=sizeof(*e);e->NumberOfDiskExtents=1;e->Extents[0].DiskNumber=0;e->Extents[0].StartingOffset.QuadPart=((h==(HANDLE)3&&scenario!=2)||(h==(HANDLE)4&&scenario==5))?469762048LL:1;e->Extents[0].ExtentLength.QuadPart=15147204608LL;return 1;}
BOOL CloseHandle(HANDLE h){(void)h;return 1;}
BOOL GetVolumeInformationW(const WCHAR*p,WCHAR*l,DWORD a,DWORD*b,DWORD*c,DWORD*d,WCHAR*f,DWORD e){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;wcscpy(l,scenario==3?L"Wrong":L"Windows");wcscpy(f,L"NTFS");return 1;}
DWORD GetFileAttributesW(const WCHAR*p){if(wcsstr(p,L"Windows\\System32"))return scenario==4?INVALID_FILE_ATTRIBUTES:0;return FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;}
BOOL SetVolumeMountPointW(const WCHAR*a,const WCHAR*b){(void)a;(void)b;assigned++;return scenario!=7;}
DWORD GetLastError(void){return error;}
void ExitProcess(DWORD v){code=v;longjmp(done,1);}
#include "../tools/winpe/map-offline-windows.c"
int main(void){static const unsigned expected[]={0,0,2,2,2,2,2,3,2,2,2,2,2};for(scenario=0;scenario<13;scenario++){assigned=drives_calls=0;error=0;if(!setjmp(done))mainCRTStartup();if(code!=expected[scenario]||(scenario!=0&&scenario!=7&&assigned)||(scenario==0&&assigned!=1)){fprintf(stderr,"scenario %u exit %u assigns %u\n",scenario,code,assigned);return 1;}}puts("offline volume mapper guards: PASS");return 0;}
