#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
static jmp_buf done;
static int mode, queries, opened, flushed, closed, status;
LONG RegOpenKeyExW(HKEY h,const WCHAR *n,DWORD o,DWORD a,HKEY *k) {
 (void)h;(void)n;(void)o;(void)a; ++queries;
 if(mode==1 && queries==1) { *k=(HKEY)1; return 0; }
 if(mode==2 && queries==1) return 5;
 return ERROR_FILE_NOT_FOUND;
}
LONG RegCloseKey(HKEY k) { (void)k; return 0; }
HANDLE CreateFileW(const WCHAR *p,DWORD a,DWORD s,void *x,DWORD d,DWORD f,HANDLE t) {
 (void)x;(void)f;(void)t; ++opened; assert(!wcscmp(p,L"\\\\.\\C:")); assert(a==GENERIC_WRITE && s==3 && d==OPEN_EXISTING);
 return mode==3 ? INVALID_HANDLE_VALUE : (HANDLE)2;
}
BOOL FlushFileBuffers(HANDLE h) { assert(h==(HANDLE)2); ++flushed; return mode!=4; }
BOOL CloseHandle(HANDLE h) { assert(h==(HANDLE)2); ++closed; return mode!=5; }
HANDLE GetStdHandle(DWORD d) { (void)d; return (HANDLE)3; }
BOOL WriteFile(HANDLE h,const void *p,DWORD n,DWORD *w,void *o) { (void)h;(void)p;(void)o; *w=n; return 1; }
void ExitProcess(DWORD s) {status=(int)s; longjmp(done,1);}
void mainCRTStartup(void);
int main(void) {
 for(mode=0;mode<=5;mode++) {
  queries=opened=flushed=closed=0;status=-1;
  if (!setjmp(done)) mainCRTStartup();
  if(mode==0) { assert(status==0 && queries==2 && opened==1 && flushed==1 && closed==1); }
  if(mode==1 || mode==2) assert(status==2 && opened==0 && flushed==0);
  if(mode==3) assert(status==3 && opened==1 && flushed==0);
  if(mode==4 || mode==5) assert(status==4 && flushed==1 && closed==1);
 }
 puts("volume flush: loaded/unknown hive, open, flush and close failures reject: OK");
}
