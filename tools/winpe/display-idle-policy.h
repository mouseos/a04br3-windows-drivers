/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Explicit development opt-in. Original AC/DC values remain in the service log directory.
 * Power APIs: PowerGetActiveScheme, PowerRead/WriteAC/DCValueIndex,
 * PowerSetActiveScheme (powrprof.h, Windows 8.1 SDK).
 */
#ifndef A04_DEVELOPMENT_DISPLAY_ON
#define A04_DEVELOPMENT_DISPLAY_ON 0
#endif
#if A04_DEVELOPMENT_DISPLAY_ON
static DWORD development_display_idle(void)
{
 typedef DWORD (WINAPI *GetScheme)(HKEY,GUID **);
 typedef DWORD (WINAPI *ReadIndex)(HKEY,const GUID *,const GUID *,const GUID *,LPDWORD);
 typedef DWORD (WINAPI *WriteIndex)(HKEY,const GUID *,const GUID *,const GUID *,DWORD);
 typedef DWORD (WINAPI *SetScheme)(HKEY,const GUID *);
 static const GUID video={0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
 static const GUID timeout={0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
 struct {DWORD bytes,version;GUID scheme;DWORD ac,dc;} before;
 WCHAR sys[MAX_PATH],dll[MAX_PATH];HMODULE mod=NULL;GUID *scheme=NULL,*current=NULL;
 GetScheme get;ReadIndex read_ac,read_dc;WriteIndex write_ac,write_dc;SetScheme activate;
 DWORD e=0,ac=0,dc=0,rollback=0,q,observed=0;int attempted=0;
 memset(&before,0,sizeof(before));before.bytes=sizeof(before);before.version=1;
 q=GetSystemDirectoryW(sys,MAX_PATH);if(!q){e=GetLastError();goto done;}if(q>=MAX_PATH){e=ERROR_INSUFFICIENT_BUFFER;goto done;}
 e=path(dll,sys,L"powrprof.dll");if(e)goto done;
 mod=LoadLibraryW(dll);if(!mod){e=GetLastError();goto done;}
 get=(GetScheme)GetProcAddress(mod,"PowerGetActiveScheme");
 read_ac=(ReadIndex)GetProcAddress(mod,"PowerReadACValueIndex");read_dc=(ReadIndex)GetProcAddress(mod,"PowerReadDCValueIndex");
 write_ac=(WriteIndex)GetProcAddress(mod,"PowerWriteACValueIndex");write_dc=(WriteIndex)GetProcAddress(mod,"PowerWriteDCValueIndex");
 activate=(SetScheme)GetProcAddress(mod,"PowerSetActiveScheme");
 if(!get||!read_ac||!read_dc||!write_ac||!write_dc||!activate){e=ERROR_PROC_NOT_FOUND;goto done;}
 e=get(NULL,&scheme);if(e)goto done;if(!scheme){e=ERROR_INVALID_DATA;goto done;}
 before.scheme=*scheme;e=read_ac(NULL,scheme,&video,&timeout,&before.ac);if(e)goto done;
 e=read_dc(NULL,scheme,&video,&timeout,&before.dc);if(e)goto done;
 e=save_blob(L"display-idle-before.bin",&before,sizeof(before));if(e)goto done;
 e=get(NULL,&current);if(e)goto done;if(!current||memcmp(current,scheme,sizeof(*scheme))){e=ERROR_RETRY;goto done;}
 LocalFree(current);current=NULL;
 attempted=1;e=write_ac(NULL,scheme,&video,&timeout,0);if(e)goto done;
 e=write_dc(NULL,scheme,&video,&timeout,0);if(e)goto done;
 e=get(NULL,&current);if(e)goto done;if(!current||memcmp(current,scheme,sizeof(*scheme))){e=ERROR_RETRY;goto done;}
 LocalFree(current);current=NULL;e=activate(NULL,scheme);if(e)goto done;
 e=read_ac(NULL,scheme,&video,&timeout,&ac);if(e)goto done;observed|=1;
 e=read_dc(NULL,scheme,&video,&timeout,&dc);if(e)goto done;observed|=2;
 if(ac||dc){e=ERROR_INVALID_DATA;goto done;}
 e=get(NULL,&current);if(e)goto done;if(!current||memcmp(current,scheme,sizeof(*scheme)))e=ERROR_RETRY;
 done:
 if(current){LocalFree(current);current=NULL;}
 if(e&&attempted){
  rollback=write_ac(NULL,scheme,&video,&timeout,before.ac);
  q=write_dc(NULL,scheme,&video,&timeout,before.dc);if(q&&!rollback)rollback=q;
  q=get(NULL,&current);if(!q&&!current)q=ERROR_INVALID_DATA;if(q&&!rollback)rollback=q;
  if(!q&&current&&!memcmp(current,scheme,sizeof(*scheme))){q=activate(NULL,scheme);if(q&&!rollback)rollback=q;}
  record(0,0x2b1,rollback);
 }
 record(0,0x2b0,e);record(0,0x2b6,observed);record(0,0x2b2,ac);record(0,0x2b3,dc);
 if(current)LocalFree(current);if(scheme)LocalFree(scheme);if(mod)FreeLibrary(mod);return e;
}
#endif
