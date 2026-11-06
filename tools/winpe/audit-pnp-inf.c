/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only audit of an already-loaded RAM copy of SYSTEM.  Missing values,
 * unknown encodings and missing catalog paths are reported, never guessed. */
#include <windows.h>
#include <wincrypt.h>
static BYTE buf[8192];static HCRYPTPROV prov;
static void out(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void wide(const WCHAR*s){char c[2];c[1]=0;while(*s){c[0]=(*s>=32&&*s<127)?(char)*s:'?';out(c);s++;}}
static int safe(const WCHAR*s,int slash){unsigned n=0;for(;*s;s++,n++)if(!((*s>=L'0'&&*s<=L'9')||(*s>=L'a'&&*s<=L'z')||(*s>=L'A'&&*s<=L'Z')||*s==L'.'||*s==L'_'||*s==L'-'||*s==L'{'||*s==L'}'||*s==L'&'||(slash&&*s==L'\\')))return 0;return n>0&&n<128;}
static void hex(DWORD v){const char*h="0123456789ABCDEF";char s[9];int i;for(i=0;i<8;i++)s[i]=h[(v>>(28-i*4))&15];s[8]=0;out(s);}
static int hash(const WCHAR*path){HANDLE f;HCRYPTHASH h=0;BYTE d[32];DWORD n,z=32,i;BY_HANDLE_FILE_INFORMATION inf;int ok=0;
 f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(f==INVALID_HANDLE_VALUE){out("OPEN_ERROR_");hex(GetLastError());return 0;}
 if(!GetFileInformationByHandle(f,&inf)||inf.nNumberOfLinks!=1||(inf.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE)))goto done;
 if(!CryptCreateHash(prov,CALG_SHA_256,0,0,&h))goto done;
 do{if(!ReadFile(f,buf,sizeof(buf),&n,0)|| (n&&!CryptHashData(h,buf,n,0)))goto done;}while(n);
 if(!CryptGetHashParam(h,HP_HASHVAL,d,&z,0)||z!=32)goto done;
 for(i=0;i<32;i++){const char*q="0123456789ABCDEF";char t[3];t[0]=q[d[i]>>4];t[1]=q[d[i]&15];t[2]=0;out(t);}ok=1;
 done:if(h)CryptDestroyHash(h);CloseHandle(f);if(!ok)out("HASH_FAILED");return ok;}
static int value(HKEY k,const WCHAR*name,WCHAR*data,DWORD bytes){DWORD type=0,n=bytes;LONG x=RegQueryValueExW(k,name,0,&type,(BYTE*)data,&n);if(x!=ERROR_SUCCESS||type!=REG_SZ||n<sizeof(WCHAR)||n>bytes||data[n/sizeof(WCHAR)-1]!=0)return 0;return 1;}
static void instance(HKEY root,const WCHAR*cs,const WCHAR*id,const WCHAR*name){WCHAR path[512],driver[128],infname[128],inffile[320],cat[128],catfile[384];HKEY k,c;LONG rc;
 lstrcpyW(path,cs);lstrcatW(path,L"\\Enum\\");lstrcatW(path,id);lstrcatW(path,L"\\");lstrcatW(path,name);
 out("PNP_INSTANCE=");wide(id);out("\\");wide(name);out("\r\n");rc=RegOpenKeyExW(root,path,0,KEY_READ,&k);if(rc!=ERROR_SUCCESS){out("PNP_INSTANCE_OPEN_FAILED\r\n");return;}
 if(!value(k,L"Driver",driver,sizeof(driver))||!safe(driver,1)){out("PNP_DRIVER_REFERENCE_UNRESOLVED\r\n");RegCloseKey(k);return;}
 out("PNP_DRIVER_REFERENCE=");wide(driver);out("\r\n");RegCloseKey(k);
 lstrcpyW(path,cs);lstrcatW(path,L"\\Control\\Class\\");lstrcatW(path,driver);
 if(RegOpenKeyExW(root,path,0,KEY_READ,&c)!=ERROR_SUCCESS){out("PNP_CLASS_KEY_UNRESOLVED\r\n");return;}
 if(!value(c,L"InfPath",infname,sizeof(infname))||!safe(infname,0)){out("PNP_INF_REFERENCE_UNRESOLVED\r\n");RegCloseKey(c);return;}RegCloseKey(c);
 lstrcpyW(inffile,L"C:\\Windows\\INF\\");lstrcatW(inffile,infname);out("PNP_INF=");wide(infname);out(" SHA256=");hash(inffile);out("\r\n");
 cat[0]=0;GetPrivateProfileStringW(L"Version",L"CatalogFile.NTARM",L"",cat,128,inffile);if(!cat[0])GetPrivateProfileStringW(L"Version",L"CatalogFile",L"",cat,128,inffile);
 if(!safe(cat,0)){out("PNP_CATALOG_REFERENCE_UNRESOLVED\r\n");return;}
 lstrcpyW(catfile,L"C:\\Windows\\System32\\CatRoot\\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}\\");lstrcatW(catfile,cat);out("PNP_CATALOG=");wide(cat);out(" SHA256=");hash(catfile);out("\r\n");
}
static void device(HKEY root,const WCHAR*cs,const WCHAR*id){WCHAR path[320],name[128];DWORD index=0,n;HKEY k;LONG rc;
 lstrcpyW(path,cs);lstrcatW(path,L"\\Enum\\");lstrcatW(path,id);rc=RegOpenKeyExW(root,path,0,KEY_READ,&k);if(rc!=ERROR_SUCCESS){out("PNP_ENUM_ABSENT_OR_ERROR=");wide(id);out("\r\n");return;}
 for(;;){n=128;rc=RegEnumKeyExW(k,index++,name,&n,0,0,0,0);if(rc==ERROR_NO_MORE_ITEMS)break;if(rc!=ERROR_SUCCESS||!safe(name,0)){out("PNP_ENUM_INCOMPLETE\r\n");break;}instance(root,cs,id,name);}RegCloseKey(k);
}
void mainCRTStartup(void){HKEY root,sel;DWORD current=0,n=sizeof(current),type=0;WCHAR cs[]=L"ControlSet000";
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"A04AUDITSYS",0,KEY_READ,&root)!=ERROR_SUCCESS){out("PNP_RAM_HIVE_NOT_LOADED\r\n");ExitProcess(2);}
 if(RegOpenKeyExW(root,L"Select",0,KEY_READ,&sel)!=ERROR_SUCCESS){RegCloseKey(root);ExitProcess(2);}
 if(RegQueryValueExW(sel,L"Current",0,&type,(BYTE*)&current,&n)!=ERROR_SUCCESS||type!=REG_DWORD||n!=4||current<1||current>999){out("PNP_CONTROLSET_UNKNOWN\r\n");RegCloseKey(sel);RegCloseKey(root);ExitProcess(2);}RegCloseKey(sel);
 cs[10]=(WCHAR)(L'0'+current/100);cs[11]=(WCHAR)(L'0'+current/10%10);cs[12]=(WCHAR)(L'0'+current%10);out("PNP_CONTROLSET=");wide(cs);out("\r\n");
 if(!CryptAcquireContextW(&prov,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){RegCloseKey(root);ExitProcess(3);}
 device(root,cs,L"ACPI\\MOU0004");device(root,cs,L"ACPI\\MOU0005");device(root,cs,L"ACPI\\BEN0001");device(root,cs,L"MT8167\\BU21025");
 CryptReleaseContext(prov,0);RegCloseKey(root);out("PNP_SCOPE_KNOWN_IDS_ONLY_CATALOG_MEMBERSHIP_UNVERIFIED\r\n");ExitProcess(0);}
