/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only WinPE observation of MSDC service, ACPI PnP and disk GPT. */
#include <windows.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <wincrypt.h>
static BYTE layout[65536],hashbuf[4096];
static void out(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD v){const char*h="0123456789ABCDEF";char b[9];unsigned i;for(i=0;i<8;i++)b[i]=h[(v>>(28-4*i))&15];b[8]=0;out(b);}
static void hex64(ULONGLONG v){hex((DWORD)(v>>32));hex((DWORD)v);}
static void wide(const WCHAR*s,unsigned limit){char c[2];c[1]=0;while(*s&&limit--){c[0]=(*s>=32&&*s<127)?(char)*s:'?';out(c);s++;}}
static int file_pin(HCRYPTPROV provider,const WCHAR*path,const char*label,const char*expected){HANDLE f;HCRYPTHASH h=0;BYTE d[32];DWORD n,z=32,i;int ok=0;const char*digits="0123456789abcdef";
 out(label);out("=");f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(f==INVALID_HANDLE_VALUE){out("OPEN_ERROR\r\n");return 0;}
 {BY_HANDLE_FILE_INFORMATION info;if(!GetFileInformationByHandle(f,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE)))goto done;}
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h))goto done;
 do{if(!ReadFile(f,hashbuf,sizeof(hashbuf),&n,0)||(n&&!CryptHashData(h,hashbuf,n,0)))goto done;}while(n);
 if(!CryptGetHashParam(h,HP_HASHVAL,d,&z,0)||z!=32)goto done;for(i=0;i<32;i++)if(digits[d[i]>>4]!=expected[2*i]||digits[d[i]&15]!=expected[2*i+1]){out("MISMATCH\r\n");goto done;}out("MATCH\r\n");ok=1;
 done:if(h)CryptDestroyHash(h);if(!CloseHandle(f))ok=0;if(!ok)out("MSDC_PIN_REJECTED\r\n");return ok;}
static int package(void){HCRYPTPROV p=0;int ok=1;if(!CryptAcquireContextW(&p,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){out("MSDC_PACKAGE_HASH_PROVIDER_FAILED\r\n");return 0;}
 if(!file_pin(p,L"X:\\a04br3\\a04-msdc-probe.inf","MSDC_PIN_INF","9bbf73d73c6dcad9f392e294a2cee7ca23209f80e4d36801e6b6a5781acb7192"))ok=0;
 if(!file_pin(p,L"X:\\a04br3\\mt8167-msdc.sys","MSDC_PIN_SYS","28115a3ba588d3354e30faca4b37a7de28dcc0b288c7354e2dd3b1ddf3ef416f"))ok=0;
 if(!file_pin(p,L"X:\\a04br3\\mt8167-msdc.cat","MSDC_PIN_CAT","e88b0514f64cdca47f5b12d6351df51518fbbf47c6253ff469609e96ef3f021c"))ok=0;
 CryptReleaseContext(p,0);return ok;}
static void service(const WCHAR*name){SC_HANDLE m,s;SERVICE_STATUS st;out("MSDC_SERVICE=");wide(name,80);out(" ");m=OpenSCManagerW(0,0,SC_MANAGER_CONNECT);if(!m){out("SCM_ERROR=");hex(GetLastError());out("\r\n");return;}s=OpenServiceW(m,name,SERVICE_QUERY_STATUS);if(!s){out("OPEN_ERROR=");hex(GetLastError());out("\r\n");CloseServiceHandle(m);return;}if(QueryServiceStatus(s,&st)){out("STATE=");hex(st.dwCurrentState);out(" WIN32=");hex(st.dwWin32ExitCode);out(" SERVICE=");hex(st.dwServiceSpecificExitCode);}else{out("QUERY_ERROR=");hex(GetLastError());}out("\r\n");CloseServiceHandle(s);CloseServiceHandle(m);}
static int pnp(void){HKEY k;WCHAR name[200],id[256];DWORD i=0,n,flags,problem;LONG e;DEVINST node;CONFIGRET r;int seen=0,started=0;
 e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Enum\\ACPI\\MOU0001",0,KEY_READ,&k);if(e!=ERROR_SUCCESS){out("MSDC_PNP_ENUM_ERROR=");hex(e);out("\r\n");return 0;}
 for(;;){n=200;e=RegEnumKeyExW(k,i++,name,&n,0,0,0,0);if(e==ERROR_NO_MORE_ITEMS)break;if(e!=ERROR_SUCCESS||n>190){out("MSDC_PNP_ENUM_INCOMPLETE=");hex(e);out("\r\n");break;}
 lstrcpyW(id,L"ACPI\\MOU0001\\");lstrcatW(id,name);out("MSDC_PNP_INSTANCE=");wide(id,250);out(" ");r=CM_Locate_DevNodeW(&node,id,CM_LOCATE_DEVNODE_NORMAL);if(r==CR_SUCCESS){r=CM_Get_DevNode_Status(&flags,&problem,node,0);if(r==CR_SUCCESS){out("STATUS=");hex(flags);out(" PROBLEM=");hex(problem);if((flags&DN_STARTED)&&problem==0)started++;}else{out("STATUS_CR=");hex(r);}}else{out("LOCATE_CR=");hex(r);}out("\r\n");seen++;}
 RegCloseKey(k);out("MSDC_PNP_INSTANCES=");hex(seen);out(" STARTED=");hex(started);out("\r\n");return started>0;}
static void disk(void){HANDLE h;DWORD n=0,i,count;DRIVE_LAYOUT_INFORMATION_EX*d=(DRIVE_LAYOUT_INFORMATION_EX*)layout;
 h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE){out("MSDC_DISK0_OPEN_ERROR=");hex(GetLastError());out("\r\n");return;}
 if(!DeviceIoControl(h,IOCTL_DISK_GET_DRIVE_LAYOUT_EX,0,0,layout,sizeof(layout),&n,0)||n<offsetof(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry)){out("MSDC_DISK0_LAYOUT_ERROR=");hex(GetLastError());out("\r\n");CloseHandle(h);return;}
 count=d->PartitionCount;out("MSDC_DISK0_LAYOUT_STYLE=");hex(d->PartitionStyle);out(" COUNT=");hex(count);out(" BYTES=");hex(n);out("\r\n");
 if(count>(n-(DWORD)offsetof(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry))/sizeof(PARTITION_INFORMATION_EX)||count>128){out("MSDC_DISK0_LAYOUT_TRUNCATED\r\n");CloseHandle(h);return;}
 for(i=0;i<count;i++){PARTITION_INFORMATION_EX*p=&d->PartitionEntry[i];out("MSDC_GPT_ENTRY=");hex(p->PartitionNumber);out(" STYLE=");hex(p->PartitionStyle);out(" OFFSET=");hex64((ULONGLONG)p->StartingOffset.QuadPart);out(" LENGTH=");hex64((ULONGLONG)p->PartitionLength.QuadPart);if(p->PartitionStyle==PARTITION_STYLE_GPT){out(" NAME=");wide(p->Gpt.Name,36);}out("\r\n");}CloseHandle(h);}
void mainCRTStartup(void){int started;WCHAR *c=GetCommandLineW(),*last=c;while(*c){if(*c==L' ')last=c+1;c++;}if(!lstrcmpW(last,L"--verify-package")){ExitProcess(package()?0:2);}if(!lstrcmpW(last,L"--wait")){out("MSDC_PNP_WAIT_MS=00001388\r\n");Sleep(5000);}service(L"a04msdcboot");service(L"a04msdcprobe");started=pnp();disk();out("MSDC_OBSERVATION_READ_ONLY\r\n");ExitProcess(started?0:4);}
