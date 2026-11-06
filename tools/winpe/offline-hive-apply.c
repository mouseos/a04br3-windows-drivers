/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Limited offline hive update. A validated durable journal and exclusive
 * ownership are mandatory. This does not install drivers or update PnP. */
#include <windows.h>
#include <wincrypt.h>

#define MAX_CERT 16384
static BYTE certs[2][MAX_CERT],blob[MAX_CERT+44],existing_blob[MAX_CERT+44],hash_buffer[4096];
static DWORD cert_len[2];static HCRYPTPROV provider;
static const WCHAR *certpath[2]={L"X:\\a04br3\\driver-trust\\toprgu.cer",L"X:\\a04br3\\driver-trust\\shared.cer"};
static const WCHAR *certpin[2]={L"X:\\a04br3\\driver-trust\\toprgu-cert.sha256",L"X:\\a04br3\\driver-trust\\shared-cert.sha256"};
static const WCHAR *sys[3]={L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys",L"C:\\Windows\\System32\\drivers\\bu21025.sys"};
static const WCHAR *syspin[3]={L"X:\\a04br3\\driver-trust\\toprgu-sys.sha256",L"X:\\a04br3\\driver-trust\\shared-sys.sha256",L"X:\\a04br3\\driver-trust\\touch-sys.sha256"};
static void say(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static int equal(const BYTE *a,const BYTE *b,DWORD n){DWORD i;BYTE d=0;for(i=0;i<n;++i)d|=a[i]^b[i];return d==0;}
static int read_exact_file(const WCHAR *p,BYTE *b,DWORD cap,DWORD *len){HANDLE h;DWORD n=0,x;BYTE extra;int ok=0;h=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(h==INVALID_HANDLE_VALUE)return 0;while(n<cap){if(!ReadFile(h,b+n,cap-n,&x,0))goto done;if(!x)break;n+=x;}if(n==cap){if(!ReadFile(h,&extra,1,&x,0)||x)goto done;}*len=n;ok=1;done:if(!CloseHandle(h))ok=0;return ok;}
static int digest_mem(ALG_ID alg,const BYTE *data,DWORD size,BYTE *out,DWORD expected){HCRYPTHASH h;DWORD n=expected;int ok;if(!CryptCreateHash(provider,alg,0,0,&h))return 0;ok=CryptHashData(h,data,size,0)&&CryptGetHashParam(h,HP_HASHVAL,out,&n,0)&&n==expected;CryptDestroyHash(h);return ok;}
static int digest_file(const WCHAR *path,BYTE *out){HANDLE f;HCRYPTHASH h;DWORD n,size=32;int ok=0;f=CreateFileW(path,GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(f==INVALID_HANDLE_VALUE)return 0;if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h)){CloseHandle(f);return 0;}for(;;){if(!ReadFile(f,hash_buffer,sizeof(hash_buffer),&n,0))goto done;if(!n)break;if(!CryptHashData(h,hash_buffer,n,0))goto done;}ok=CryptGetHashParam(h,HP_HASHVAL,out,&size,0)&&size==32;done:CryptDestroyHash(h);if(!CloseHandle(f))ok=0;return ok;}
static int verify_pin(const WCHAR *target,const WCHAR *pin){BYTE got[32],want[32];DWORD n;if(!read_exact_file(pin,want,sizeof(want),&n)||n!=32||!digest_file(target,got))return 0;return equal(got,want,32);}
static int privilege(const WCHAR *name){HANDLE t;TOKEN_PRIVILEGES p;LUID l;DWORD e;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&t))return 0;if(!LookupPrivilegeValueW(0,name,&l)){CloseHandle(t);return 0;}p.PrivilegeCount=1;p.Privileges[0].Luid=l;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);if(!AdjustTokenPrivileges(t,FALSE,&p,0,0,0)){CloseHandle(t);return 0;}e=GetLastError();CloseHandle(t);return e==ERROR_SUCCESS;}
static int check_inputs(void){unsigned i;PCCERT_CONTEXT ctx;for(i=0;i<3;++i)if(!verify_pin(sys[i],syspin[i]))return 0;for(i=0;i<2;++i){if(!verify_pin(certpath[i],certpin[i])||!read_exact_file(certpath[i],certs[i],MAX_CERT,&cert_len[i])||!cert_len[i])return 0;ctx=CertCreateCertificateContext(X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,certs[i],cert_len[i]);if(!ctx)return 0;CertFreeCertificateContext(ctx);}return 1;}
static int set_toprgu(void){HKEY root=0,select=0,service=0;DWORD current=0,n=sizeof(current),type=0,start=0;LONG rc;int ok=0;WCHAR name[64];lstrcpyW(name,L"ControlSet000\\Services\\Mt8167Toprgu");
 rc=RegLoadKeyW(HKEY_LOCAL_MACHINE,L"A04TXSYS",L"C:\\Windows\\System32\\config\\SYSTEM");if(rc!=ERROR_SUCCESS)return 0;
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"A04TXSYS",0,KEY_READ,&root)!=ERROR_SUCCESS)goto done;
 if(RegOpenKeyExW(root,L"Select",0,KEY_READ,&select)!=ERROR_SUCCESS)goto done;
 if(RegQueryValueExW(select,L"Current",0,&type,(BYTE*)&current,&n)!=ERROR_SUCCESS||type!=REG_DWORD||n!=4||current<1||current>999)goto done;
 name[10]=(WCHAR)(L'0'+current/100);name[11]=(WCHAR)(L'0'+(current/10)%10);name[12]=(WCHAR)(L'0'+current%10);
 if(RegOpenKeyExW(root,name,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&service)!=ERROR_SUCCESS)goto done;
 {WCHAR image[256];DWORD service_type=0;DWORD sz=sizeof(image);type=0;
  if(RegQueryValueExW(service,L"ImagePath",0,&type,(BYTE*)image,&sz)!=ERROR_SUCCESS||(type!=REG_SZ&&type!=REG_EXPAND_SZ)||sz<2||sz>sizeof(image)||sz%2||image[sz/2-1]!=0)goto done;
  if(lstrcmpiW(image,L"\\SystemRoot\\System32\\drivers\\mt8167-toprgu.sys") && lstrcmpiW(image,L"system32\\drivers\\mt8167-toprgu.sys") && lstrcmpiW(image,L"%SystemRoot%\\System32\\drivers\\mt8167-toprgu.sys"))goto done;
  sz=sizeof(service_type);type=0;if(RegQueryValueExW(service,L"Type",0,&type,(BYTE*)&service_type,&sz)!=ERROR_SUCCESS||type!=REG_DWORD||sz!=4||service_type!=1)goto done;
 }
 n=sizeof(start);type=0;if(RegQueryValueExW(service,L"Start",0,&type,(BYTE*)&start,&n)!=ERROR_SUCCESS||type!=REG_DWORD||n!=4||start>4)goto done;
 start=0;if(RegSetValueExW(service,L"Start",0,REG_DWORD,(BYTE*)&start,4)!=ERROR_SUCCESS||RegFlushKey(service)!=ERROR_SUCCESS)goto done;
 n=sizeof(start);type=0;if(RegQueryValueExW(service,L"Start",0,&type,(BYTE*)&start,&n)!=ERROR_SUCCESS||type!=REG_DWORD||n!=4||start!=0)goto done;ok=1;
 done:if(service)RegCloseKey(service);if(select)RegCloseKey(select);if(root)RegCloseKey(root);if(RegUnLoadKeyW(HKEY_LOCAL_MACHINE,L"A04TXSYS")!=ERROR_SUCCESS)ok=0;return ok;}
static void put32(BYTE *p,DWORD x){p[0]=(BYTE)x;p[1]=(BYTE)(x>>8);p[2]=(BYTE)(x>>16);p[3]=(BYTE)(x>>24);}
static int add_cert(HKEY root,const WCHAR *store,unsigned i){BYTE sha1[20];WCHAR path[160],thumb[41];static const WCHAR digits[]=L"0123456789ABCDEF";HKEY parent=0,key=0;DWORD j,disp=0,type=0,n;LONG rc;int ok=0;
 if(!digest_mem(CALG_SHA1,certs[i],cert_len[i],sha1,20))return 0;
 for(j=0;j<20;++j){thumb[j*2]=digits[sha1[j]>>4];thumb[j*2+1]=digits[sha1[j]&15];}thumb[40]=0;
 lstrcpyW(path,L"Microsoft\\SystemCertificates\\");lstrcatW(path,store);lstrcatW(path,L"\\Certificates");
 if(RegOpenKeyExW(root,path,0,KEY_CREATE_SUB_KEY,&parent)!=ERROR_SUCCESS)return 0;
 if(RegCreateKeyExW(parent,thumb,0,0,REG_OPTION_NON_VOLATILE,KEY_QUERY_VALUE|KEY_SET_VALUE,0,&key,&disp)!=ERROR_SUCCESS)goto done;
 put32(blob,3);put32(blob+4,1);put32(blob+8,20);for(j=0;j<20;++j)blob[12+j]=sha1[j];put32(blob+32,32);put32(blob+36,1);put32(blob+40,cert_len[i]);for(j=0;j<cert_len[i];++j)blob[44+j]=certs[i][j];n=44+cert_len[i];
 if(disp==REG_OPENED_EXISTING_KEY){DWORD size=sizeof(existing_blob);rc=RegQueryValueExW(key,L"Blob",0,&type,existing_blob,&size);if(rc!=ERROR_SUCCESS||type!=REG_BINARY||size!=n||!equal(existing_blob,blob,n))goto done;}
 else if(RegSetValueExW(key,L"Blob",0,REG_BINARY,blob,n)!=ERROR_SUCCESS)goto done;
 if(RegFlushKey(key)!=ERROR_SUCCESS)goto done;ok=1;
 done:if(key)RegCloseKey(key);if(parent)RegCloseKey(parent);return ok;}
static int set_trust(void){HKEY root=0;LONG rc;int ok=0;rc=RegLoadKeyW(HKEY_LOCAL_MACHINE,L"A04TXSW",L"C:\\Windows\\System32\\config\\SOFTWARE");if(rc!=ERROR_SUCCESS)return 0;
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"A04TXSW",0,KEY_READ|KEY_WRITE,&root)!=ERROR_SUCCESS)goto done;
 if(!add_cert(root,L"Root",0))goto done;
#ifdef A04_HIVE_FAULT_INJECTION
 {char x[4];if(GetEnvironmentVariableA("A04_HIVE_FAIL_AFTER_FIRST_CERT",x,sizeof(x))==1&&x[0]=='1')goto done;}
#endif
 if(!add_cert(root,L"TrustedPublisher",0)||!add_cert(root,L"Root",1)||!add_cert(root,L"TrustedPublisher",1))goto done;ok=1;
 done:if(root)RegCloseKey(root);if(RegUnLoadKeyW(HKEY_LOCAL_MACHINE,L"A04TXSW")!=ERROR_SUCCESS)ok=0;return ok;}
#ifdef A04_HIVE_FIXTURE
static int journal_ready(void){char x[4];return GetEnvironmentVariableA("A04_FIXTURE_JOURNAL",x,sizeof(x))==1&&x[0]=='1';}
#else
static WCHAR probe_cmd[]=L"\"X:\\a04br3\\offline-backup.exe\" --armed";
static int journal_ready(void){STARTUPINFOW si;PROCESS_INFORMATION pi;BYTE *p;DWORD i,code=1;BOOL ok;
 p=(BYTE*)&si;for(i=0;i<sizeof(si);++i)p[i]=0;p=(BYTE*)&pi;for(i=0;i<sizeof(pi);++i)p[i]=0;si.cb=sizeof(si);
 ok=CreateProcessW(L"X:\\a04br3\\offline-backup.exe",probe_cmd,0,0,FALSE,0,0,0,&si,&pi);if(!ok)return 0;
 if(WaitForSingleObject(pi.hProcess,INFINITE)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code==0;
}
#endif
static int mode(void){const WCHAR *p=GetCommandLineW();if(*p==L'"'){++p;while(*p&&*p!=L'"')++p;if(!*p)return -1;++p;}else while(*p&&*p!=L' '&&*p!=L'\t')++p;while(*p==L' '||*p==L'\t')++p;if(!lstrcmpW(p,L"--check"))return 0;if(!lstrcmpW(p,L"--apply"))return 1;return -1;}
void mainCRTStartup(void){int m=mode();if(m<0||!privilege(L"SeBackupPrivilege")||!privilege(L"SeRestorePrivilege")||!CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){say("HIVE_APPLY_PRECONDITION_FAILED\r\n");ExitProcess(2);}
 if(!check_inputs()){say("HIVE_APPLY_INPUT_REJECTED\r\n");ExitProcess(3);}if(!m){say("HIVE_APPLY_INPUTS_MATCH\r\n");CryptReleaseContext(provider,0);ExitProcess(0);}
 if(!journal_ready()){say("HIVE_APPLY_JOURNAL_REJECTED\r\n");ExitProcess(5);}
 if(!set_toprgu()){say("HIVE_APPLY_FAILED_RECOVERY_REQUIRED\r\n");ExitProcess(4);}
#ifdef A04_HIVE_FAULT_INJECTION
 {char x[4];if(GetEnvironmentVariableA("A04_HIVE_FAIL_AFTER_SYSTEM",x,sizeof(x))==1&&x[0]=='1'){say("HIVE_APPLY_FIXTURE_FAILURE\r\n");ExitProcess(4);}}
#endif
 if(!set_trust()){say("HIVE_APPLY_FAILED_RECOVERY_REQUIRED\r\n");ExitProcess(4);}CryptReleaseContext(provider,0);say("HIVE_APPLY_COMPLETE_DRIVER_PNP_UNVERIFIED\r\n");ExitProcess(0);}
