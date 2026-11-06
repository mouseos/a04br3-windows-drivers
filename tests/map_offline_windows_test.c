/* Actual mapper code with a simulated volume namespace, never a real disk. */
#include <stdio.h>
#include <setjmp.h>
#include <wchar.h>
#include <stdlib.h>
#include "windows.h"
#include "winioctl.h"
static jmp_buf done;static unsigned scenario,code,indexn,assigned,removed,drives_calls,infos,sleeps;static DWORD error;
HANDLE GetStdHandle(DWORD n){(void)n;return (HANDLE)1;}
BOOL WriteFile(HANDLE h,const void*p,DWORD n,DWORD*w,void*o){(void)h;(void)p;(void)o;*w=n;return 1;}
int CompareStringOrdinal(const WCHAR*a,int al,const WCHAR*b,int bl,BOOL f){(void)f;return al==bl&&!wcsncmp(a,b,al)?CSTR_EQUAL:0;}
int lstrlenW(const WCHAR*s){return (int)wcslen(s);} WCHAR*lstrcpyW(WCHAR*a,const WCHAR*b){return wcscpy(a,b);} WCHAR*lstrcatW(WCHAR*a,const WCHAR*b){return wcscat(a,b);} int lstrcmpiW(const WCHAR*a,const WCHAR*b){return wcscmp(a,b);}
UINT GetWindowsDirectoryW(WCHAR*p,UINT n){(void)n;wcscpy(p,scenario==12?L"C:\\Windows":L"X:\\Windows");return 10;}
LONG RegOpenKeyExW(HKEY h,const WCHAR*p,DWORD a,DWORD b,HKEY*k){(void)h;(void)p;(void)a;(void)b;*k=(HKEY)3;return 0;} LONG RegCloseKey(HKEY h){(void)h;return 0;}
DWORD GetLogicalDrives(void){drives_calls++;if(scenario==1)return 4;if(scenario==9&&drives_calls>1)return 4;return 16;}
BOOL GetVolumeNameForVolumeMountPointW(const WCHAR*p,WCHAR*out,DWORD n){(void)n;wcscpy(out, !wcscmp(p,L"C:\\") ? L"\\\\?\\VOL1\\" : L"\\\\?\\RAM\\");return scenario!=10;}
HANDLE FindFirstVolumeW(WCHAR*p,DWORD n){(void)n;indexn=0;wcscpy(p,L"\\\\?\\RAM\\");return scenario==11?INVALID_HANDLE_VALUE:(HANDLE)2;}
BOOL FindNextVolumeW(HANDLE h,WCHAR*p,DWORD n){(void)h;(void)n;indexn++;if(scenario==6&&indexn==2){error=5;return 0;}if(indexn<=2){wcscpy(p,indexn==1?L"\\\\?\\VOL1\\":L"\\\\?\\VOL2\\");return 1;}error=ERROR_NO_MORE_FILES;return 0;}
BOOL FindVolumeClose(HANDLE h){(void)h;return 1;}
HANDLE CreateFileW(const WCHAR*p,DWORD a,DWORD b,void*c,DWORD d,DWORD e,HANDLE f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;if(wcsstr(p,L"PhysicalDrive0"))return (HANDLE)5;if(scenario==8)return INVALID_HANDLE_VALUE;return (HANDLE)(intptr_t)(wcschr(p,L'1')?3:4);}
BOOL DeviceIoControl(HANDLE h,DWORD c,void*a,DWORD b,void*out,DWORD os,DWORD*n,void*x){(void)a;(void)b;(void)os;(void)x;
 if(c==IOCTL_DISK_GET_DRIVE_LAYOUT_EX){DRIVE_LAYOUT_INFORMATION_EX*d=out;PARTITION_INFORMATION_EX*p=&d->PartitionEntry[0];GUID disk={0xe347eb68,0x4ff8,0x5143,{0xa3,0x34,0x08,0x86,0x80,0x3f,0xd8,0xfa}},type={0xebd0a0a2,0xb9e5,0x4433,{0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7}},id={0xf31443dd,0x566a,0x5dbe,{0x86,0xc6,0xeb,0xc1,0xb3,0x0b,0x65,0xa1}};*n=sizeof(*d);d->PartitionStyle=1;d->PartitionCount=1;d->Gpt.DiskId=disk;p->PartitionStyle=1;p->StartingOffset.QuadPart=469762048LL;p->PartitionLength.QuadPart=15147204608LL;p->PartitionNumber=20;p->Gpt.PartitionType=type;p->Gpt.PartitionId=id;wcscpy(p->Gpt.Name,L"Windows");if(scenario==14)p->Gpt.PartitionId.Data1=0;if(scenario==18)d->Gpt.DiskId.Data1=0;if(scenario==19)wcscpy(p->Gpt.Name,L"Wrong");return 1;}
 if(c==IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS){VOLUME_DISK_EXTENTS*e=out;*n=sizeof(*e);e->NumberOfDiskExtents=1;e->Extents[0].DiskNumber=0;e->Extents[0].StartingOffset.QuadPart=((h==(HANDLE)3&&scenario!=2)||(h==(HANDLE)4&&scenario==5))?469762048LL:1;e->Extents[0].ExtentLength.QuadPart=15147204608LL;return 1;}abort();}
BOOL SetFilePointerEx(HANDLE h,LARGE_INTEGER a,LARGE_INTEGER*b,DWORD c){(void)h;(void)a;(void)b;(void)c;return 1;}
BOOL ReadFile(HANDLE h,void*p,DWORD n,DWORD*done,void*x){unsigned char*b=p;(void)h;(void)x;for(DWORD i=0;i<n;i++)b[i]=0;b[3]='N';b[4]='T';b[5]='F';b[6]='S';b[7]=b[8]=b[9]=b[10]=' ';b[12]=2;b[30]=14;b[40]=1;b[510]=0x55;b[511]=0xaa;if(scenario==15)b[3]='X';if(scenario==20)b[30]=0;*done=n;return 1;}
void Sleep(DWORD n){(void)n;sleeps++;}
BOOL CloseHandle(HANDLE h){(void)h;return 1;}
BOOL GetVolumeInformationW(const WCHAR*p,WCHAR*l,DWORD a,DWORD*b,DWORD*c,DWORD*d,WCHAR*f,DWORD e){(void)a;(void)b;(void)c;(void)d;(void)e;infos++;if((((scenario>=13&&scenario<=16)||(scenario>=18&&scenario<=21))&&wcscmp(p,L"C:\\"))||scenario==16){error=ERROR_NOT_READY;return 0;}wcscpy(l,scenario==3||scenario==17||(scenario==21&&!wcscmp(p,L"C:\\"))?L"Wrong":L"Windows");wcscpy(f,L"NTFS");return 1;}
DWORD GetFileAttributesW(const WCHAR*p){if(wcsstr(p,L"Windows\\System32"))return scenario==4?INVALID_FILE_ATTRIBUTES:0;return FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM;}
BOOL DeleteVolumeMountPointW(const WCHAR*p){(void)p;removed++;return 1;}
BOOL SetVolumeMountPointW(const WCHAR*a,const WCHAR*b){(void)a;(void)b;assigned++;return scenario!=7;}
DWORD GetLastError(void){return error;}
void ExitProcess(DWORD v){code=v;longjmp(done,1);}
#include "../tools/winpe/map-offline-windows.c"
int main(void){static const unsigned expected[]={0,0,4,2,2,2,2,3,2,2,2,2,2,0,2,2,2,2,2,2,2,2};for(scenario=0;scenario<22;scenario++){assigned=removed=drives_calls=infos=sleeps=0;error=0;if(!setjmp(done))mainCRTStartup();if(code!=expected[scenario]||(scenario!=0&&scenario!=7&&scenario!=13&&scenario!=16&&scenario!=21&&assigned)||((scenario==0||scenario==13)&&assigned!=1)||(scenario==16&&(removed!=1||sleeps!=2))||(scenario==21&&(assigned!=1||removed!=1))){fprintf(stderr,"scenario %u exit %u assigns %u\n",scenario,code,assigned);return 1;}}puts("offline volume mapper guards: PASS");return 0;}
