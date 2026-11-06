/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only preliminary evidence. This does not audit registry or PnP. */
#include <windows.h>
#include <winioctl.h>
#include <wincrypt.h>
static void text(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void number(DWORD n){char s[9];unsigned i;const char *d="0123456789ABCDEF";for(i=0;i<8;i++)s[i]=d[(n>>(28-4*i))&15];s[8]=0;text(s);}
static BYTE b[8192];
static int filehash(HCRYPTPROV p,const WCHAR *path,const char *label){HANDLE f;HCRYPTHASH h=0;BYTE digest[32];DWORD n,len=sizeof(digest),i,err;int rc=0;
 text(label);text("=");f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);
 if(f==INVALID_HANDLE_VALUE){err=GetLastError();text("OPEN_ERROR_");number(err);text("\r\n");return 0;}
 {BY_HANDLE_FILE_INFORMATION info;if(!GetFileInformationByHandle(f,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_OFFLINE))){text("UNSAFE\r\n");goto out;}}
 if(!CryptCreateHash(p,CALG_SHA_256,0,0,&h)){text("HASH_CREATE_ERROR\r\n");goto out;}
 for(;;){if(!ReadFile(f,b,sizeof(b),&n,0)){text("READ_ERROR\r\n");goto out;}if(!n)break;if(!CryptHashData(h,b,n,0)){text("HASH_ERROR\r\n");goto out;}}
 if(!CryptGetHashParam(h,HP_HASHVAL,digest,&len,0)||len!=32){text("HASH_RESULT_ERROR\r\n");goto out;}
 for(i=0;i<32;i++){const char *d="0123456789ABCDEF";char x[3];x[0]=d[digest[i]>>4];x[1]=d[digest[i]&15];x[2]=0;text(x);}text("\r\n");rc=1;
 out:if(h)CryptDestroyHash(h);if(!CloseHandle(f))rc=0;return rc;}
void mainCRTStartup(void){HANDLE v;VOLUME_DISK_EXTENTS e;DWORD n=0,flags=0,serial=0;WCHAR fs[32];HCRYPTPROV p=0;int ok=1;
 if(!GetVolumeInformationW(L"C:\\",0,0,&serial,0,0,fs,32)||lstrcmpiW(fs,L"NTFS")||!serial){text("AUDIT_IDENTITY_REJECTED\r\n");ExitProcess(2);}
 v=CreateFileW(L"\\\\.\\C:",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(v==INVALID_HANDLE_VALUE){text("AUDIT_VOLUME_OPEN_FAILED\r\n");ExitProcess(2);}
 if(!DeviceIoControl(v,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,0,0,&e,sizeof(e),&n,0)||n<sizeof(e)||e.NumberOfDiskExtents!=1||e.Extents[0].DiskNumber!=0||e.Extents[0].StartingOffset.QuadPart!=469762048LL||e.Extents[0].ExtentLength.QuadPart!=15147204608LL){CloseHandle(v);text("AUDIT_EXTENT_REJECTED\r\n");ExitProcess(2);}
 text("AUDIT_NTFS_SERIAL=");number(serial);text("\r\n");n=0;
 if(!DeviceIoControl(v,FSCTL_IS_VOLUME_DIRTY,0,0,&flags,sizeof(flags),&n,0)||n!=sizeof(flags)){text("AUDIT_DIRTY_QUERY_FAILED\r\n");ok=0;}else{text("AUDIT_DIRTY_FLAGS=");number(flags);text("\r\n");}
 if(!CloseHandle(v))ok=0;if(!CryptAcquireContextW(&p,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){text("AUDIT_HASH_PROVIDER_FAILED\r\n");ExitProcess(3);}
 if(!filehash(p,L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys","AUDIT_TOPRGU_SHA256"))ok=0;
 if(!filehash(p,L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys","AUDIT_SHARED_SHA256"))ok=0;
 if(!filehash(p,L"C:\\Windows\\System32\\drivers\\bu21025.sys","AUDIT_BU21025_SHA256"))ok=0;
 CryptReleaseContext(p,0);text("AUDIT_PRELIMINARY_ONLY_NO_PNP_OR_INF\r\n");ExitProcess(ok?0:4);}
