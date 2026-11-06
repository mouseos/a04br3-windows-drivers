/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copy only to a new RAM directory. Files remain open without write/delete
 * sharing until two enumerations and a bytewise reread are complete.
 * Directory enumeration is not an atomic snapshot; callers must fail closed
 * unless external exclusion of concurrent writers is independently assured. */
#include <windows.h>
#define MAX_ITEMS 99
#define MAX_NAME 128
static WCHAR names[MAX_ITEMS][MAX_NAME];static HANDLE src[MAX_ITEMS];static unsigned count;
static BYTE a[8192],b[8192];
static void say(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
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
static int copyone(unsigned i){WCHAR dst[320];HANDLE f=INVALID_HANDLE_VALUE;DWORD n,m,w;BY_HANDLE_FILE_INFORMATION before,after;int ok=0;
 if(!GetFileInformationByHandle(src[i],&before)||before.nNumberOfLinks!=1||before.nFileSizeHigh||before.nFileSizeLow>134217728u||before.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ENCRYPTED|FILE_ATTRIBUTE_OFFLINE))return 0;
 path(dst,L"X:\\a04br3\\audit-system\\",names[i]);f=CreateFileW(dst,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;
 for(;;){if(!ReadFile(src[i],a,sizeof(a),&n,0))goto done;if(!n)break;if(!WriteFile(f,a,n,&w,0)||w!=n)goto done;}
 if(SetFilePointer(src[i],0,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER||SetFilePointer(f,0,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER)goto done;
 do{if(!ReadFile(src[i],a,sizeof(a),&n,0)||!ReadFile(f,b,sizeof(b),&m,0)||n!=m)goto done;for(w=0;w<n;w++)if(a[w]!=b[w])goto done;}while(n);
 if(!GetFileInformationByHandle(src[i],&after)||!sameinfo(&before,&after))goto done;ok=1;
 done:if(!CloseHandle(f))ok=0;return ok;}
void mainCRTStartup(void){static const WCHAR*fixed[]={L"SYSTEM",L"SYSTEM.LOG",L"SYSTEM.LOG1",L"SYSTEM.LOG2",L"SOFTWARE",L"SOFTWARE.LOG",L"SOFTWARE.LOG1",L"SOFTWARE.LOG2"};unsigned i;DWORD attr,error;WCHAR p[320];int ok=0;
 if(GetFileAttributesW(L"X:\\a04br3\\audit-system")!=INVALID_FILE_ATTRIBUTES){say("HIVE_COPY_DEST_EXISTS\r\n");ExitProcess(2);}count=0;
 for(i=0;i<8;i++){path(p,L"C:\\Windows\\System32\\config\\",fixed[i]);attr=GetFileAttributesW(p);if(attr==INVALID_FILE_ATTRIBUTES){error=GetLastError();if((i==0||i==4)||error!=ERROR_FILE_NOT_FOUND){say("HIVE_COPY_FIXED_MISSING_OR_ERROR\r\n");ExitProcess(2);}continue;}if(!add(fixed[i]))ExitProcess(2);}
 if(!sidecars(0)){say("HIVE_COPY_ENUMERATION_FAILED\r\n");ExitProcess(2);}for(i=0;i<count;i++)src[i]=INVALID_HANDLE_VALUE;
 for(i=0;i<count;i++){path(p,L"C:\\Windows\\System32\\config\\",names[i]);src[i]=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);if(src[i]==INVALID_HANDLE_VALUE){say("HIVE_COPY_SOURCE_BUSY_OR_MISSING\r\n");goto done;}}
 if(!CreateDirectoryW(L"X:\\a04br3\\audit-system",0)||!CreateDirectoryW(L"X:\\a04br3\\audit-system\\TxR",0)){say("HIVE_COPY_DEST_CREATE_FAILED\r\n");goto done;}
 for(i=0;i<count;i++)if(!copyone(i)){say("HIVE_COPY_COMPARE_FAILED\r\n");goto done;}
 if(!sidecars(1)){say("HIVE_COPY_SET_CHANGED\r\n");goto done;}
 for(i=0;i<count;i++){path(p,L"C:\\Windows\\System32\\config\\",names[i]);if(GetFileAttributesW(p)==INVALID_FILE_ATTRIBUTES){say("HIVE_COPY_SET_CHANGED\r\n");goto done;}}
 for(i=0;i<8;i++){path(p,L"C:\\Windows\\System32\\config\\",fixed[i]);attr=GetFileAttributesW(p);if((attr==INVALID_FILE_ATTRIBUTES && GetLastError()!=ERROR_FILE_NOT_FOUND)||((attr==INVALID_FILE_ATTRIBUTES)!=(indexof(fixed[i])<0))){say("HIVE_COPY_FIXED_SET_CHANGED\r\n");goto done;}}
 say("HIVE_COPY_REJECTED_DIRECTORY_EXCLUSION_UNPROVEN\r\n");ok=1;
 done:for(i=0;i<count;i++)if(src[i]!=INVALID_HANDLE_VALUE)CloseHandle(src[i]);ExitProcess(ok?4:3);}
