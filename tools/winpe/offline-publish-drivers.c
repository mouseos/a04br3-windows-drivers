/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Publish exactly three already-staged SYS files under an armed journal.
 * Does not install INF/CAT, establish PnP, commit, or release the volume. */
#include <windows.h>
#include <wincrypt.h>
#include <sddl.h>
static BYTE buffer[65536],old_security[65536],new_security[65536];static HCRYPTPROV provider;
static const WCHAR *dst[3]={L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys",L"C:\\Windows\\System32\\drivers\\bu21025.sys"};
static const WCHAR *tmp[3]={L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys.a04restore",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys.a04restore",L"C:\\Windows\\System32\\drivers\\bu21025.sys.a04restore"};
static const WCHAR *pin[3]={L"X:\\a04br3\\driver-trust\\toprgu-sys.sha256",L"X:\\a04br3\\driver-trust\\shared-sys.sha256",L"X:\\a04br3\\driver-trust\\touch-sys.sha256"};
static void say(const char*s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD v){static const char d[]="0123456789ABCDEF";char b[9];unsigned i;for(i=0;i<8;i++)b[i]=d[(v>>(28-4*i))&15];b[8]=0;say(b);}
static int equal(const BYTE*a,const BYTE*b,DWORD n){BYTE x=0;DWORD i;for(i=0;i<n;i++)x|=a[i]^b[i];return x==0;}
static int privilege(const WCHAR *name){HANDLE t;TOKEN_PRIVILEGES p;LUID l;DWORD e;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&t))return 0;if(!LookupPrivilegeValueW(0,name,&l)){CloseHandle(t);return 0;}p.PrivilegeCount=1;p.Privileges[0].Luid=l;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);if(!AdjustTokenPrivileges(t,FALSE,&p,0,0,0)){CloseHandle(t);return 0;}e=GetLastError();CloseHandle(t);return e==ERROR_SUCCESS;}
static int get_pin(unsigned i,BYTE *expected){HANDLE h;BYTE b;DWORD n;int ok;h=CreateFileW(pin[i],GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(h==INVALID_HANDLE_VALUE)return 0;ok=ReadFile(h,expected,32,&n,0)&&n==32&&ReadFile(h,&b,1,&n,0)&&n==0;if(!CloseHandle(h))ok=0;return ok;}
static int pinned(const WCHAR*path,const BYTE *expected){HANDLE f;HCRYPTHASH h=0;BY_HANDLE_FILE_INFORMATION info;BYTE digest[32];DWORD n,len=32;int ok=0;
 f=CreateFileW(path,GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(f==INVALID_HANDLE_VALUE)return 0;
 if(!GetFileInformationByHandle(f,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE)))goto done;
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h))goto done;
 do{if(!ReadFile(f,buffer,sizeof(buffer),&n,0)||(n&&!CryptHashData(h,buffer,n,0)))goto done;}while(n);
 if(CryptGetHashParam(h,HP_HASHVAL,digest,&len,0)&&len==32&&equal(digest,expected,32))ok=1;
 done:if(h)CryptDestroyHash(h);if(!CloseHandle(f))ok=0;return ok;}
#ifndef A04_PUBLISH_FIXTURE
static int run(const WCHAR*app,WCHAR*cmd){STARTUPINFOW si;PROCESS_INFORMATION pi;BYTE*p;DWORD i,rc=1;BOOL ok;p=(BYTE*)&si;for(i=0;i<sizeof(si);i++)p[i]=0;p=(BYTE*)&pi;for(i=0;i<sizeof(pi);i++)p[i]=0;si.cb=sizeof(si);ok=CreateProcessW(app,cmd,0,0,FALSE,0,0,0,&si,&pi);if(!ok)return 0;if(WaitForSingleObject(pi.hProcess,INFINITE)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&rc);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return rc==0;}
static int guards(void){static WCHAR a[]=L"\"X:\\a04br3\\offline-backup.exe\" --armed",b[]=L"\"X:\\a04br3\\offline-baseline-guard.exe\"",c[]=L"\"X:\\a04br3\\offline-pnp-baseline.exe\"",d[]=L"\"X:\\a04br3\\offline-hive-apply.exe\" --payload-check",e[]=L"\"X:\\a04br3\\offline-signature-check.exe\"";
 return run(L"X:\\a04br3\\offline-backup.exe",a)&&run(L"X:\\a04br3\\offline-baseline-guard.exe",b)&&run(L"X:\\a04br3\\offline-pnp-baseline.exe",c)&&run(L"X:\\a04br3\\offline-hive-apply.exe",d)&&run(L"X:\\a04br3\\offline-signature-check.exe",e);}
static int flush_volume(void){HANDLE h;int ok;h=CreateFileW(L"\\\\.\\C:",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;ok=FlushFileBuffers(h)!=0;if(!CloseHandle(h))ok=0;return ok;}
static void fault(unsigned i){(void)i;}
#else
static int guards(void){char s[4];return GetEnvironmentVariableA("A04_PUBLISH_FIXTURE_ARMED",s,sizeof(s))==1&&s[0]=='1';}
static int flush_volume(void){return 1;}
static void fault(unsigned i){char s[4];if(GetEnvironmentVariableA("A04_PUBLISH_FIXTURE_FAIL",s,sizeof(s))==1&&s[0]==(char)('0'+i))ExitProcess(98);}
#endif
static int security(const WCHAR*path,BYTE *data,DWORD *size){SECURITY_INFORMATION which=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|SACL_SECURITY_INFORMATION;DWORD n=0;if(!GetFileSecurityW(path,which,data,65536,&n)||!n||n>65536)return 0;*size=n;return 1;}
static int security_equal(PSECURITY_DESCRIPTOR a,PSECURITY_DESCRIPTOR b){SECURITY_DESCRIPTOR_CONTROL ca,cb;DWORD ra=0,rb=0,na=0,nb=0;LPWSTR sa=0,sb=0;int ok=0;SECURITY_INFORMATION which=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|SACL_SECURITY_INFORMATION;
 if(!GetSecurityDescriptorControl(a,&ca,&ra)||!GetSecurityDescriptorControl(b,&cb,&rb))return 0;if(ca!=cb||ra!=rb){say("DRIVER_PUBLISH_SD_CONTROL_OLD=");hex(ca);say(" NEW=");hex(cb);say("\r\n");return 0;}
 if(!ConvertSecurityDescriptorToStringSecurityDescriptorW(a,SDDL_REVISION_1,which,&sa,&na)||!ConvertSecurityDescriptorToStringSecurityDescriptorW(b,SDDL_REVISION_1,which,&sb,&nb))goto done;
 ok=na==nb&&!lstrcmpW(sa,sb);
 done:if(sa)LocalFree(sa);if(sb)LocalFree(sb);return ok;}
static int publish(unsigned i,const BYTE *expected){DWORD oldattr,newattr,oldlen=0,newlen=0,revision=0;SECURITY_DESCRIPTOR_CONTROL control=0;SECURITY_INFORMATION restore_info=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;BOOL ok;
 if(i==1){oldattr=GetFileAttributesW(dst[i]);if(oldattr!=INVALID_FILE_ATTRIBUTES||GetLastError()!=ERROR_FILE_NOT_FOUND)return 0;ok=MoveFileExW(tmp[i],dst[i],MOVEFILE_WRITE_THROUGH);}
 else{oldattr=GetFileAttributesW(dst[i]);if(oldattr==INVALID_FILE_ATTRIBUTES||(oldattr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||!security(dst[i],old_security,&oldlen))return 0;ok=ReplaceFileW(dst[i],tmp[i],0,REPLACEFILE_WRITE_THROUGH,0,0);}
 if(!ok){say("DRIVER_PUBLISH_REPLACE_ERROR=");hex(GetLastError());say("\r\n");return 0;}if(i!=1){if(!GetSecurityDescriptorControl((PSECURITY_DESCRIPTOR)old_security,&control,&revision)){say("DRIVER_PUBLISH_SECURITY_CONTROL_ERROR\r\n");return 0;}if(control&SE_SACL_PRESENT)restore_info|=SACL_SECURITY_INFORMATION;if(!SetFileSecurityW(dst[i],restore_info,(PSECURITY_DESCRIPTOR)old_security)){say("DRIVER_PUBLISH_SECURITY_RESTORE_ERROR=");hex(GetLastError());say("\r\n");return 0;}}if(i!=1&&!SetFileAttributesW(dst[i],oldattr)){say("DRIVER_PUBLISH_ATTR_RESTORE_ERROR=");hex(GetLastError());say("\r\n");return 0;}if(!flush_volume()){say("DRIVER_PUBLISH_FLUSH_ERROR\r\n");return 0;}if(!pinned(dst[i],expected)){say("DRIVER_PUBLISH_HASH_ERROR\r\n");return 0;}
 if(i!=1){newattr=GetFileAttributesW(dst[i]);if(newattr!=oldattr){say("DRIVER_PUBLISH_ATTR_CHANGED\r\n");return 0;}if(!security(dst[i],new_security,&newlen)){say("DRIVER_PUBLISH_SECURITY_READ_ERROR\r\n");return 0;}if(!security_equal((PSECURITY_DESCRIPTOR)old_security,(PSECURITY_DESCRIPTOR)new_security)){say("DRIVER_PUBLISH_SECURITY_CHANGED\r\n");return 0;}}
 return 1;}
void mainCRTStartup(void){BYTE expected[3][32];unsigned i;if(!guards()){say("DRIVER_PUBLISH_GUARD_REJECTED_NO_WRITE\r\n");ExitProcess(2);}
 if(!privilege(L"SeSecurityPrivilege")||!privilege(L"SeRestorePrivilege")||!privilege(L"SeBackupPrivilege")||!CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){say("DRIVER_PUBLISH_PRECONDITION_FAILED\r\n");ExitProcess(2);}
 for(i=0;i<3;i++)if(!get_pin(i,expected[i])||!pinned(tmp[i],expected[i])){say("DRIVER_PUBLISH_STAGE_REJECTED_NO_WRITE\r\n");CryptReleaseContext(provider,0);ExitProcess(2);}
 for(i=0;i<3;i++){if(!publish(i,expected[i])){say("DRIVER_PUBLISH_FAILED_JOURNAL_RESTORE_REQUIRED\r\n");CryptReleaseContext(provider,0);ExitProcess(3);}fault(i);}
 CryptReleaseContext(provider,0);say("DRIVER_PUBLISH_COMPLETE_PNP_AND_RT_POLICY_UNPROVEN\r\n");ExitProcess(0);}
