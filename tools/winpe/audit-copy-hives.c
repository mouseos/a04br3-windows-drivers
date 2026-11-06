/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copy only to a new RAM directory. Files remain open without write/delete
 * sharing until two enumerations and a bytewise reread are complete.
 * Directory enumeration is not an atomic snapshot. This is an observational
 * read-only audit; it must never authorize a servicing transaction. */
#include <windows.h>
#include <wincrypt.h>
#define MAX_ITEMS 99
#define MAX_NAME 128
static WCHAR names[MAX_ITEMS][MAX_NAME];static HANDLE src[MAX_ITEMS];static unsigned count;
static BYTE a[8192],b[8192],digests[MAX_ITEMS][32];static HCRYPTPROV provider;static int created;
static void say(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void filename(const WCHAR*s){char c[2];c[1]=0;while(*s){c[0]=(*s>=32&&*s<127)?(char)*s:'?';say(c);s++;}}
static int is_winpe(void){
#ifdef A04_TEST_FIXTURE
 return 1;
#else
 WCHAR p[MAX_PATH];HKEY key;UINT n=GetWindowsDirectoryW(p,MAX_PATH);LONG e;DWORD xs=0,cs=0;
 if(!n||n>=MAX_PATH||lstrcmpiW(p,L"X:\\Windows"))return 0;
 if(!GetVolumeInformationW(L"X:\\",0,0,&xs,0,0,0,0)||!GetVolumeInformationW(L"C:\\",0,0,&cs,0,0,0,0)||!xs||!cs||xs==cs)return 0;
 e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_READ,&key);
 if(e!=ERROR_SUCCESS)return 0;RegCloseKey(key);return 1;
#endif
}
static void digestprint(BYTE*d){const char*h="0123456789ABCDEF";char z[3];unsigned j;z[2]=0;for(j=0;j<32;j++){z[0]=h[d[j]>>4];z[1]=h[d[j]&15];say(z);}}
static int ends(const WCHAR*s,const WCHAR*t){int a1=lstrlenW(s),b1=lstrlenW(t);return a1>=b1&&!lstrcmpiW(s+a1-b1,t);}
static int valid(const WCHAR*s){unsigned n=0;int brace=0;for(;*s;s++,n++){WCHAR c=*s;if(c==L'}')brace=1;if(!((c>=L'0'&&c<=L'9')||(c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||c==L'{'||c==L'}'||c==L'.'||c==L'-'||c==L'_'))return 0;}return n>=8&&n<118&&brace;}
static void path(WCHAR*p,const WCHAR*root,const WCHAR*name){lstrcpyW(p,root);lstrcatW(p,name);}
static int indexof(const WCHAR*n){unsigned i;for(i=0;i<count;i++)if(!lstrcmpiW(names[i],n))return (int)i;return -1;}
static int add(const WCHAR*n){if(count>=MAX_ITEMS||indexof(n)>=0||lstrlenW(n)>=MAX_NAME)return 0;lstrcpyW(names[count++],n);return 1;}
static int scan(const WCHAR*pattern,int txr,int verify){WIN32_FIND_DATAW d;HANDLE h;DWORD err;WCHAR n[MAX_NAME];
 h=FindFirstFileW(pattern,&d);if(h==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
 do {if(!lstrcmpW(d.cFileName,L".")||!lstrcmpW(d.cFileName,L".."))continue;
 if(d.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)||!valid(d.cFileName)||!(ends(d.cFileName,L".blf")||ends(d.cFileName,L".regtrans-ms"))){FindClose(h);return 0;}
 n[0]=0;if(txr)lstrcpyW(n,L"TxR\\");lstrcatW(n,d.cFileName);if(verify){if(indexof(n)<0){FindClose(h);return 0;}}else if(!add(n)){FindClose(h);return 0;}
 }while(FindNextFileW(h,&d));err=GetLastError();return FindClose(h)&&err==ERROR_NO_MORE_FILES;}
static int sidecars(int verify){DWORD x=GetFileAttributesW(L"C:\\Windows\\System32\\config\\TxR");if(x==INVALID_FILE_ATTRIBUTES||!(x&FILE_ATTRIBUTE_DIRECTORY)||(x&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
 return scan(L"C:\\Windows\\System32\\config\\SYSTEM{*",0,verify)&&scan(L"C:\\Windows\\System32\\config\\SOFTWARE{*",0,verify)&&scan(L"C:\\Windows\\System32\\config\\TxR\\*",1,verify);}
static int sameinfo(BY_HANDLE_FILE_INFORMATION*x,BY_HANDLE_FILE_INFORMATION*y){return x->dwVolumeSerialNumber==y->dwVolumeSerialNumber&&x->nFileIndexHigh==y->nFileIndexHigh&&x->nFileIndexLow==y->nFileIndexLow&&x->nFileSizeHigh==y->nFileSizeHigh&&x->nFileSizeLow==y->nFileSizeLow&&x->ftLastWriteTime.dwHighDateTime==y->ftLastWriteTime.dwHighDateTime&&x->ftLastWriteTime.dwLowDateTime==y->ftLastWriteTime.dwLowDateTime;}
static int copyone(unsigned i){WCHAR dst[320];HANDLE f=INVALID_HANDLE_VALUE;DWORD n,m,w,z;BYTE d1[32],d2[32],d3[32];HCRYPTHASH h1=0,h2=0,h3=0;BY_HANDLE_FILE_INFORMATION before,after;int ok=0;
 if(!GetFileInformationByHandle(src[i],&before)||before.nNumberOfLinks!=1||before.nFileSizeHigh||before.nFileSizeLow>134217728u||before.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ENCRYPTED|FILE_ATTRIBUTE_OFFLINE))return 0;
 path(dst,L"X:\\a04br3\\audit-system\\",names[i]);f=CreateFileW(dst,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h1)||!CryptCreateHash(provider,CALG_SHA_256,0,0,&h2)||!CryptCreateHash(provider,CALG_SHA_256,0,0,&h3))goto done;
 for(;;){if(!ReadFile(src[i],a,sizeof(a),&n,0))goto done;if(!n)break;if(!CryptHashData(h1,a,n,0)||!WriteFile(f,a,n,&w,0)||w!=n)goto done;}
#ifdef A04_TEST_FIXTURE
 if(i==0&&GetEnvironmentVariableW(L"A04_COPY_CORRUPT",0,0)){BYTE corrupt=0xff;if(SetFilePointer(f,0,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER||!WriteFile(f,&corrupt,1,&w,0)||w!=1)goto done;}
#endif
 if(SetFilePointer(src[i],0,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER||SetFilePointer(f,0,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER)goto done;
 do{if(!ReadFile(src[i],a,sizeof(a),&n,0)||!ReadFile(f,b,sizeof(b),&m,0)||n!=m)goto done;if(n&&(!CryptHashData(h2,a,n,0)||!CryptHashData(h3,b,n,0)))goto done;for(w=0;w<n;w++)if(a[w]!=b[w])goto done;}while(n);
 z=32;if(!CryptGetHashParam(h1,HP_HASHVAL,d1,&z,0)||z!=32)goto done;z=32;if(!CryptGetHashParam(h2,HP_HASHVAL,d2,&z,0)||z!=32)goto done;z=32;if(!CryptGetHashParam(h3,HP_HASHVAL,d3,&z,0)||z!=32)goto done;
 for(w=0;w<32;w++)if(d1[w]!=d2[w]||d1[w]!=d3[w])goto done;
 if(!GetFileInformationByHandle(src[i],&after)||!sameinfo(&before,&after))goto done;for(w=0;w<32;w++)digests[i][w]=d1[w];ok=1;
 done:if(h1)CryptDestroyHash(h1);if(h2)CryptDestroyHash(h2);if(h3)CryptDestroyHash(h3);if(!CloseHandle(f))ok=0;return ok;}
static void discard(void){unsigned i;WCHAR p[320];if(!created)return;for(i=0;i<count;i++){path(p,L"X:\\a04br3\\audit-system\\",names[i]);if(!DeleteFileW(p)&&GetLastError()!=ERROR_FILE_NOT_FOUND)say("HIVE_COPY_DISCARD_FILE_FAILED\r\n");}if(!RemoveDirectoryW(L"X:\\a04br3\\audit-system\\TxR")&&GetLastError()!=ERROR_PATH_NOT_FOUND&&GetLastError()!=ERROR_FILE_NOT_FOUND)say("HIVE_COPY_DISCARD_DIR_FAILED\r\n");if(!RemoveDirectoryW(L"X:\\a04br3\\audit-system"))say("HIVE_COPY_DISCARD_DIR_FAILED\r\n");}
void mainCRTStartup(void){static const WCHAR*fixed[]={L"SYSTEM",L"SYSTEM.LOG",L"SYSTEM.LOG1",L"SYSTEM.LOG2",L"SOFTWARE",L"SOFTWARE.LOG",L"SOFTWARE.LOG1",L"SOFTWARE.LOG2"};unsigned i;DWORD attr,error;WCHAR p[320];int ok=0;
 if(!is_winpe()){say("HIVE_COPY_NOT_WINPE\r\n");ExitProcess(2);}
 if(!CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){say("HIVE_COPY_HASH_PROVIDER_FAILED\r\n");ExitProcess(2);}
 if(GetFileAttributesW(L"X:\\a04br3\\audit-system")!=INVALID_FILE_ATTRIBUTES){say("HIVE_COPY_DEST_EXISTS\r\n");ExitProcess(2);}count=0;
 for(i=0;i<8;i++){path(p,L"C:\\Windows\\System32\\config\\",fixed[i]);attr=GetFileAttributesW(p);if(attr==INVALID_FILE_ATTRIBUTES){error=GetLastError();if((i==0||i==4)||error!=ERROR_FILE_NOT_FOUND){say("HIVE_COPY_FIXED_MISSING_OR_ERROR\r\n");ExitProcess(2);}continue;}if(!add(fixed[i]))ExitProcess(2);}
 if(!sidecars(0)){say("HIVE_COPY_ENUMERATION_FAILED\r\n");ExitProcess(2);}for(i=0;i<count;i++)src[i]=INVALID_HANDLE_VALUE;
 for(i=0;i<count;i++){path(p,L"C:\\Windows\\System32\\config\\",names[i]);src[i]=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(src[i]==INVALID_HANDLE_VALUE){say("HIVE_COPY_SOURCE_BUSY_OR_MISSING\r\n");goto done;}}
 if(!CreateDirectoryW(L"X:\\a04br3\\audit-system",0)){say("HIVE_COPY_DEST_CREATE_FAILED\r\n");goto done;}created=1;
 if(!CreateDirectoryW(L"X:\\a04br3\\audit-system\\TxR",0)){say("HIVE_COPY_DEST_CREATE_FAILED\r\n");goto done;}
 for(i=0;i<count;i++)if(!copyone(i)){say("HIVE_COPY_COMPARE_FAILED\r\n");goto done;}
 if(!sidecars(1)){say("HIVE_COPY_SET_CHANGED\r\n");goto done;}
 for(i=0;i<count;i++){path(p,L"C:\\Windows\\System32\\config\\",names[i]);if(GetFileAttributesW(p)==INVALID_FILE_ATTRIBUTES){say("HIVE_COPY_SET_CHANGED\r\n");goto done;}}
 for(i=0;i<8;i++){path(p,L"C:\\Windows\\System32\\config\\",fixed[i]);attr=GetFileAttributesW(p);if((attr==INVALID_FILE_ATTRIBUTES && GetLastError()!=ERROR_FILE_NOT_FOUND)||((attr==INVALID_FILE_ATTRIBUTES)!=(indexof(fixed[i])<0))){say("HIVE_COPY_FIXED_SET_CHANGED\r\n");goto done;}}
 for(i=0;i<count;i++){say("HIVE_COPY_SHA256 ");filename(names[i]);say(" ");digestprint(digests[i]);say("\r\n");}
 say("HIVE_COPY_VERIFIED_READ_ONLY_NONATOMIC_SNAPSHOT_UNPROVEN\r\n");ok=1;
 done:for(i=0;i<count;i++)if(src[i]!=INVALID_HANDLE_VALUE)CloseHandle(src[i]);if(!ok)discard();CryptReleaseContext(provider,0);ExitProcess(ok?0:3);}
