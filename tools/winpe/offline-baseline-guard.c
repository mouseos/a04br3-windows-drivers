/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Verify the observed installed files before any servicing. This check alone
 * never authorizes an update or establishes exclusive ownership. */
#include <windows.h>
#include <winioctl.h>
#include <wincrypt.h>

static BYTE buffer[65536];
static HCRYPTPROV provider;
static void say(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD v){static const char d[]="0123456789ABCDEF";char b[9];unsigned i;for(i=0;i<8;i++)b[i]=d[(v>>(28-4*i))&15];b[8]=0;say(b);}
static int equal(const BYTE*a,const BYTE*b,unsigned n){BYTE x=0;unsigned i;for(i=0;i<n;i++)x|=a[i]^b[i];return x==0;}
static int pin(const WCHAR *path,const BYTE *expected,const char *tag){HANDLE f;BY_HANDLE_FILE_INFORMATION a,b;HCRYPTHASH h=0;BYTE digest[32];DWORD n,size=32;int ok=0;
 f=CreateFileW(path,GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);
 if(f==INVALID_HANDLE_VALUE){say(tag);say(" OPEN_ERROR=");hex(GetLastError());say("\r\n");return 0;}
 if(!GetFileInformationByHandle(f,&a)||a.nNumberOfLinks!=1||(a.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE)))goto done;
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h))goto done;
 do{if(!ReadFile(f,buffer,sizeof(buffer),&n,0)||(n&&!CryptHashData(h,buffer,n,0)))goto done;}while(n);
 if(!GetFileInformationByHandle(f,&b)||a.nFileSizeHigh!=b.nFileSizeHigh||a.nFileSizeLow!=b.nFileSizeLow||CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime))goto done;
 if(!CryptGetHashParam(h,HP_HASHVAL,digest,&size,0)||size!=32||!equal(digest,expected,32))goto done;ok=1;
 done:if(h)CryptDestroyHash(h);if(!CloseHandle(f))ok=0;say(tag);say(ok?" MATCH\r\n":" REJECTED\r\n");return ok;}
static int absent(const WCHAR *path){HANDLE f=CreateFileW(path,GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);DWORD e;if(f!=INVALID_HANDLE_VALUE){CloseHandle(f);say("BASELINE_SHARED_PRESENT_REJECTED\r\n");return 0;}e=GetLastError();if(e!=ERROR_FILE_NOT_FOUND){say("BASELINE_SHARED_OPEN_ERROR=");hex(e);say("\r\n");return 0;}say("BASELINE_SHARED_ABSENT\r\n");return 1;}
static int volume(void){WCHAR fs[32];DWORD serial=0,n=0,flags=0;HANDLE h;VOLUME_DISK_EXTENTS e;int ok=0;
 if(!GetVolumeInformationW(L"C:\\",0,0,&serial,0,0,fs,32)||serial!=0x1631a497||lstrcmpiW(fs,L"NTFS"))return 0;
 h=CreateFileW(L"\\\\.\\C:",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;
 if(!DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,0,0,&e,sizeof(e),&n,0)||n<sizeof(e)||e.NumberOfDiskExtents!=1||e.Extents[0].DiskNumber!=0||e.Extents[0].StartingOffset.QuadPart!=469762048LL||e.Extents[0].ExtentLength.QuadPart!=15147204608LL)goto done;
 if(!DeviceIoControl(h,FSCTL_IS_VOLUME_DIRTY,0,0,&flags,sizeof(flags),&n,0)||n!=sizeof(flags)||flags!=0)goto done;ok=1;
 done:if(!CloseHandle(h))ok=0;return ok;}
static const BYTE top[32]={0x6f,0xc5,0x68,0x29,0xbd,0xc9,0x34,0x89,0xa0,0x88,0x8a,0x39,0x5f,0xa6,0xf8,0x73,0x6c,0x4f,0xf3,0x4c,0xca,0xa3,0x51,0x62,0xbb,0x6d,0x94,0x0d,0x20,0x69,0x5f,0xc1};
static const BYTE touch[32]={0x3c,0x57,0x1c,0x84,0x87,0x2a,0xd6,0xfe,0x59,0xb2,0x1e,0xb7,0xbb,0x20,0x14,0xd2,0xec,0x28,0x6e,0x80,0x35,0xc0,0xa8,0x0d,0x5f,0x54,0x1a,0xbf,0x0a,0xa3,0x1b,0x0f};
static const BYTE inf33[32]={0xc2,0x65,0xa5,0x5a,0x63,0x0d,0x32,0xd1,0x9d,0xb4,0xee,0x6b,0xa6,0x65,0xb6,0x9e,0x35,0x8c,0x6f,0x74,0xe0,0x51,0x08,0xdc,0x60,0xe5,0x54,0xb3,0xc6,0x2a,0x42,0x5d};
static const BYTE inf35[32]={0x9b,0xe0,0xdf,0x79,0x2a,0x48,0x93,0x0d,0x6d,0x65,0x64,0x11,0xed,0x1c,0x7a,0xba,0xea,0xf1,0x46,0x80,0x42,0x4b,0x82,0x77,0x94,0xd8,0x37,0x5c,0x54,0xcb,0x18,0xdd};
void mainCRTStartup(void){WCHAR win[128];HKEY key;int ok=1;
 if(!GetWindowsDirectoryW(win,128)||lstrcmpiW(win,L"X:\\Windows")||RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_READ,&key)!=ERROR_SUCCESS){say("BASELINE_NOT_WINPE\r\n");ExitProcess(2);}RegCloseKey(key);
 if(!volume()){say("BASELINE_VOLUME_REJECTED\r\n");ExitProcess(2);}
 if(!CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){say("BASELINE_CRYPTO_FAILED\r\n");ExitProcess(2);}
 if(!pin(L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys",top,"BASELINE_TOPRGU"))ok=0;
 if(!pin(L"C:\\Windows\\System32\\drivers\\bu21025.sys",touch,"BASELINE_TOUCH"))ok=0;
 if(!pin(L"C:\\Windows\\INF\\oem33.inf",inf33,"BASELINE_INF33"))ok=0;
 if(!pin(L"C:\\Windows\\INF\\oem35.inf",inf35,"BASELINE_INF35"))ok=0;
 if(!absent(L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys"))ok=0;
 CryptReleaseContext(provider,0);if(!ok){say("BASELINE_REJECTED_NO_UPDATE\r\n");ExitProcess(3);}
 say("BASELINE_FILES_MATCH_ONLY_PNP_SIGNATURE_ROLLBACK_UNPROVEN\r\n");ExitProcess(0);}
