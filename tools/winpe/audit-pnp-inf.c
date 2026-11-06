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
/* Catalogs can be retained in DriverStore rather than under the INF's
 * original filename in CatRoot. Report all bounded filename matches, never
 * infer a binding or catalog membership from a name alone. */
static void catalog_store(const WCHAR *cat){WIN32_FIND_DATAW d;HANDLE h;DWORD error,count=0,seen=0,skipped=0,reported=0;WCHAR path[512];
 h=FindFirstFileW(L"C:\\Windows\\System32\\DriverStore\\FileRepository\\*",&d);
 if(h==INVALID_HANDLE_VALUE){out("PNP_CATALOG_STORE_ENUM_ERROR=");hex(GetLastError());out("\r\n");return;}
 do{DWORD attr;if(!lstrcmpW(d.cFileName,L".")||!lstrcmpW(d.cFileName,L".."))continue;
 if(++seen>2048){out("PNP_CATALOG_STORE_ENUM_LIMIT\r\n");FindClose(h);return;}
 if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))continue;
 if((d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!safe(d.cFileName,0)){++skipped;continue;}
 lstrcpyW(path,L"C:\\Windows\\System32\\DriverStore\\FileRepository\\");lstrcatW(path,d.cFileName);lstrcatW(path,L"\\");lstrcatW(path,cat);
 attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES){error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND){if(!skipped){out("PNP_CATALOG_STORE_STAT_ERROR=");hex(error);out("\r\n");}++skipped;}continue;}
 if(reported++>=8){out("PNP_CATALOG_STORE_MATCH_LIMIT ENUM_INCOMPLETE\r\n");FindClose(h);return;}
 out("PNP_CATALOG_STORE_PATH=");wide(path);out(" SHA256=");if(hash(path))++count;out("\r\n");
 }while(FindNextFileW(h,&d));error=GetLastError();if(!FindClose(h)||error!=ERROR_NO_MORE_FILES){out("PNP_CATALOG_STORE_ENUM_INCOMPLETE\r\n");return;}
 out("PNP_CATALOG_STORE_MATCH_COUNT=");hex(count);out(" MEMBERSHIP_UNVERIFIED\r\n");
 if(skipped){out("PNP_CATALOG_STORE_SKIPPED_COUNT=");hex(skipped);out(" ENUM_INCOMPLETE\r\n");}}
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
 catalog_store(cat);
}
static void device(HKEY root,const WCHAR*cs,const WCHAR*id){WCHAR path[320],name[128];DWORD index=0,n;HKEY k;LONG rc;
 lstrcpyW(path,cs);lstrcatW(path,L"\\Enum\\");lstrcatW(path,id);rc=RegOpenKeyExW(root,path,0,KEY_READ,&k);if(rc!=ERROR_SUCCESS){out("PNP_ENUM_ABSENT_OR_ERROR=");wide(id);out("\r\n");return;}
 for(;;){n=128;rc=RegEnumKeyExW(k,index++,name,&n,0,0,0,0);if(rc==ERROR_NO_MORE_ITEMS)break;if(rc!=ERROR_SUCCESS||!safe(name,0)){out("PNP_ENUM_INCOMPLETE\r\n");break;}instance(root,cs,id,name);}RegCloseKey(k);
}
static void service(HKEY root,const WCHAR*cs,const WCHAR*name){WCHAR p[320],data[512];HKEY k;DWORD n,type,v;LONG e;
 lstrcpyW(p,cs);lstrcatW(p,L"\\Services\\");lstrcatW(p,name);out("SERVICE=");wide(name);out("\r\n");e=RegOpenKeyExW(root,p,0,KEY_READ,&k);if(e!=ERROR_SUCCESS){out("SERVICE_OPEN_ERROR=");hex(e);out("\r\n");return;}
 n=sizeof(v);e=RegQueryValueExW(k,L"Start",0,&type,(BYTE*)&v,&n);out("SERVICE_START=");if(e==ERROR_SUCCESS&&type==REG_DWORD&&n==sizeof(v))hex(v);else out("UNRESOLVED");out("\r\n");
 n=sizeof(v);e=RegQueryValueExW(k,L"Type",0,&type,(BYTE*)&v,&n);out("SERVICE_TYPE=");if(e==ERROR_SUCCESS&&type==REG_DWORD&&n==sizeof(v))hex(v);else out("UNRESOLVED");out("\r\n");
 n=sizeof(data);e=RegQueryValueExW(k,L"ImagePath",0,&type,(BYTE*)data,&n);out("SERVICE_IMAGEPATH=");if(e==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&n>=2&&n<=sizeof(data)&&!(n%2)&&!data[n/2-1])wide(data);else out("UNRESOLVED");out("\r\n");RegCloseKey(k);}
void mainCRTStartup(void){HKEY root,sel;DWORD current=0,n=sizeof(current),type=0;WCHAR cs[]=L"ControlSet000";
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"A04AUDITSYS",0,KEY_READ,&root)!=ERROR_SUCCESS){out("PNP_RAM_HIVE_NOT_LOADED\r\n");ExitProcess(2);}
 if(RegOpenKeyExW(root,L"Select",0,KEY_READ,&sel)!=ERROR_SUCCESS){RegCloseKey(root);ExitProcess(2);}
 if(RegQueryValueExW(sel,L"Current",0,&type,(BYTE*)&current,&n)!=ERROR_SUCCESS||type!=REG_DWORD||n!=4||current<1||current>999){out("PNP_CONTROLSET_UNKNOWN\r\n");RegCloseKey(sel);RegCloseKey(root);ExitProcess(2);}RegCloseKey(sel);
 cs[10]=(WCHAR)(L'0'+current/100);cs[11]=(WCHAR)(L'0'+current/10%10);cs[12]=(WCHAR)(L'0'+current%10);out("PNP_CONTROLSET=");wide(cs);out("\r\n");
 if(!CryptAcquireContextW(&prov,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){RegCloseKey(root);ExitProcess(3);}
 service(root,cs,L"Mt8167Toprgu");service(root,cs,L"Mt8167Shared");service(root,cs,L"Bu21025Shared");service(root,cs,L"mshidkmdf");
 device(root,cs,L"ACPI\\MOU0004");device(root,cs,L"ACPI\\MOU0005");device(root,cs,L"ACPI\\BEN0001");device(root,cs,L"MT8167\\BU21025");
 CryptReleaseContext(prov,0);RegCloseKey(root);out("PNP_SCOPE_KNOWN_IDS_ONLY_CATALOG_MEMBERSHIP_UNVERIFIED\r\n");ExitProcess(0);}
