/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Stage exact SYS bytes into journal-owned temporary paths only.  This does
 * not replace drivers, install packages, modify PnP, or commit a journal. */
#include <windows.h>
#include <wincrypt.h>
static BYTE buffer[65536];static HCRYPTPROV provider;
static const WCHAR *src[3]={L"X:\\a04br3\\update\\mt8167-toprgu.sys",L"X:\\a04br3\\update\\mt8167-shared.sys",L"X:\\a04br3\\update\\bu21025.sys"};
static const WCHAR *tmp[3]={L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys.a04restore",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys.a04restore",L"C:\\Windows\\System32\\drivers\\bu21025.sys.a04restore"};
static const WCHAR *pin[3]={L"X:\\a04br3\\driver-trust\\toprgu-sys.sha256",L"X:\\a04br3\\driver-trust\\shared-sys.sha256",L"X:\\a04br3\\driver-trust\\touch-sys.sha256"};
static void say(const char*s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static int equal(const BYTE*a,const BYTE*b,unsigned n){BYTE x=0;unsigned i;for(i=0;i<n;i++)x|=a[i]^b[i];return x==0;}
static int hash_start(HCRYPTHASH *h){return CryptCreateHash(provider,CALG_SHA_256,0,0,h)!=0;}
static int hash_end(HCRYPTHASH h,BYTE *b){DWORD n=32;int ok=CryptGetHashParam(h,HP_HASHVAL,b,&n,0)&&n==32;CryptDestroyHash(h);return ok;}
static int get_pin(unsigned i,BYTE *expected){HANDLE h;BYTE b;DWORD n;int ok;h=CreateFileW(pin[i],GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(h==INVALID_HANDLE_VALUE)return 0;ok=ReadFile(h,expected,32,&n,0)&&n==32&&ReadFile(h,&b,1,&n,0)&&n==0;if(!CloseHandle(h))ok=0;return ok;}
static int info_ok(HANDLE h){BY_HANDLE_FILE_INFORMATION info;return GetFileInformationByHandle(h,&info)&&info.nNumberOfLinks==1&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE));}
static int write_all(HANDLE h,const BYTE*p,DWORD n){DWORD w;while(n){if(!WriteFile(h,p,n,&w,0)||!w)return 0;p+=w;n-=w;}return 1;}
static int stage(unsigned i){HANDLE in=INVALID_HANDLE_VALUE,out=INVALID_HANDLE_VALUE;HCRYPTHASH h=0;BYTE expected[32],source_hash[32],copy_hash[32];DWORD n;LARGE_INTEGER zero;int ok=0;
 if(!get_pin(i,expected))return 0;in=CreateFileW(src[i],GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(in==INVALID_HANDLE_VALUE||!info_ok(in))goto done;
 out=CreateFileW(tmp[i],GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,0);if(out==INVALID_HANDLE_VALUE||!info_ok(out)||!hash_start(&h))goto done;
 for(;;){if(!ReadFile(in,buffer,sizeof(buffer),&n,0))goto done;if(!n)break;if(!CryptHashData(h,buffer,n,0)||!write_all(out,buffer,n))goto done;}
 if(!hash_end(h,source_hash)){h=0;goto done;}h=0;if(!equal(source_hash,expected,32)||!FlushFileBuffers(out))goto done;
 zero.QuadPart=0;if(!SetFilePointerEx(out,zero,0,FILE_BEGIN)||!hash_start(&h))goto done;
 for(;;){if(!ReadFile(out,buffer,sizeof(buffer),&n,0))goto done;if(!n)break;if(!CryptHashData(h,buffer,n,0))goto done;}
 if(!hash_end(h,copy_hash)){h=0;goto done;}h=0;if(!equal(copy_hash,expected,32))goto done;ok=1;
 done:if(h)CryptDestroyHash(h);if(out!=INVALID_HANDLE_VALUE&&!CloseHandle(out))ok=0;if(in!=INVALID_HANDLE_VALUE&&!CloseHandle(in))ok=0;return ok;}
#ifndef A04_STAGE_FIXTURE
static int run(const WCHAR*app,WCHAR*cmd){STARTUPINFOW si;PROCESS_INFORMATION pi;BYTE*p;DWORD i,rc=1;BOOL ok;p=(BYTE*)&si;for(i=0;i<sizeof(si);i++)p[i]=0;p=(BYTE*)&pi;for(i=0;i<sizeof(pi);i++)p[i]=0;si.cb=sizeof(si);ok=CreateProcessW(app,cmd,0,0,FALSE,0,0,0,&si,&pi);if(!ok)return 0;if(WaitForSingleObject(pi.hProcess,INFINITE)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&rc);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return rc==0;}
static int guards(void){static WCHAR a[]=L"\"X:\\a04br3\\offline-backup.exe\" --armed",b[]=L"\"X:\\a04br3\\offline-baseline-guard.exe\"",c[]=L"\"X:\\a04br3\\offline-pnp-baseline.exe\"",d[]=L"\"X:\\a04br3\\offline-hive-apply.exe\" --payload-check",e[]=L"\"X:\\a04br3\\offline-signature-check.exe\"";
 return run(L"X:\\a04br3\\offline-backup.exe",a)&&run(L"X:\\a04br3\\offline-baseline-guard.exe",b)&&run(L"X:\\a04br3\\offline-pnp-baseline.exe",c)&&run(L"X:\\a04br3\\offline-hive-apply.exe",d)&&run(L"X:\\a04br3\\offline-signature-check.exe",e);}
static int flush_volume(void){HANDLE h;int ok;h=CreateFileW(L"\\\\.\\C:",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;ok=FlushFileBuffers(h)!=0;if(!CloseHandle(h))ok=0;return ok;}
static void fault(unsigned i){(void)i;}
#else
static int guards(void){char s[4];return GetEnvironmentVariableA("A04_STAGE_FIXTURE_ARMED",s,sizeof(s))==1&&s[0]=='1';}
static int flush_volume(void){return 1;}
static void fault(unsigned i){char s[4];if(GetEnvironmentVariableA("A04_STAGE_FIXTURE_FAIL",s,sizeof(s))==1&&s[0]==(char)('0'+i))ExitProcess(98);}
#endif
void mainCRTStartup(void){unsigned i;DWORD attr;if(!guards()){say("DRIVER_STAGE_GUARD_REJECTED_NO_WRITE\r\n");ExitProcess(2);}
 for(i=0;i<3;i++){attr=GetFileAttributesW(tmp[i]);if(attr!=INVALID_FILE_ATTRIBUTES||GetLastError()!=ERROR_FILE_NOT_FOUND){say("DRIVER_STAGE_TEMP_EXISTS_OR_UNKNOWN\r\n");ExitProcess(2);}}
 if(!CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){say("DRIVER_STAGE_CRYPTO_FAILED\r\n");ExitProcess(2);}
 for(i=0;i<3;i++){if(!stage(i)||!flush_volume()){say("DRIVER_STAGE_FAILED_JOURNAL_RESTORE_REQUIRED\r\n");CryptReleaseContext(provider,0);ExitProcess(3);}fault(i);}
 CryptReleaseContext(provider,0);say("DRIVER_STAGE_READY_NO_SYS_REPLACED_NO_PNP_UPDATE\r\n");ExitProcess(0);}
