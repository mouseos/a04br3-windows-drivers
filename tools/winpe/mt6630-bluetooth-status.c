/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One bounded standard PnP/radio observation. No registration, HCI commands,
 * device discovery, pairing, firmware/MMIO access or identity-byte output. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
void *memset(void *p,int c,size_t n){volatile unsigned char *v=p;while(n--)*v++=(unsigned char)c;return p;}
static void number(const char *name,DWORD v){char out[96],rev[16];DWORD n=0,m=0,w;while(*name&&n<70)out[n++]=*name++;out[n++]='=';do{rev[m++]=(char)('0'+v%10);v/=10;}while(v);while(m)out[n++]=rev[--m];out[n++]='\r';out[n++]='\n';WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),out,n,&w,NULL);}
static DWORD observe(DWORD *terminal){
 HDEVINFO set;DWORD i,e=0,first=0,roots=0,children=0;SP_DEVINFO_DATA d;
 set=SetupDiGetClassDevsW(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE)return GetLastError();
 for(i=0;i<512;i++){
  WCHAR ids[256];DWORD bytes=0,type=0,j;ZeroMemory(&d,sizeof(d));d.cbSize=sizeof(d);
  if(!SetupDiEnumDeviceInfo(set,i,&d)){e=GetLastError();if(e==ERROR_NO_MORE_ITEMS)e=0;break;}
  if(!SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_HARDWAREID,&type,(BYTE*)ids,sizeof(ids),&bytes)){e=GetLastError();if(e==ERROR_INVALID_DATA){e=0;continue;}break;}
  if(type!=REG_MULTI_SZ||bytes<4||bytes>sizeof(ids)||(bytes&1)||ids[bytes/2-1]||ids[bytes/2-2]){e=ERROR_INVALID_DATA;break;}
  for(j=0;j+1<bytes/2&&ids[j];){
   int root=!lstrcmpiW(ids+j,L"ROOT\\MT6630COMBO"),child=!lstrcmpiW(ids+j,L"A04BR3\\MT6630HCI");
   if(root||child){ULONG status=0,problem=0;CONFIGRET cr=CM_Get_DevNode_Status(&status,&problem,d.DevInst,0);WCHAR svc[64];DWORD used,kind;
    if(problem)*terminal=1;if(root)roots++;else children++;number(root?"ROOT_CR":"CHILD_CR",cr);number(root?"ROOT_STATUS":"CHILD_STATUS",status);number(root?"ROOT_PROBLEM":"CHILD_PROBLEM",problem);
    if(!SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_SERVICE,&kind,(BYTE*)svc,sizeof(svc),&used))e=GetLastError();
    else if(kind!=REG_SZ||used<2||used>sizeof(svc)||(used&1)||svc[used/2-1]||lstrcmpiW(svc,root?L"Mt6630ComboBluetooth":L"BthMini"))e=ERROR_REVISION_MISMATCH;
    if(!first)first=e?e:(cr!=CR_SUCCESS||!(status&DN_STARTED)||problem?ERROR_NOT_READY:0);
   }
   while(j<bytes/2&&ids[j])j++;j++;
  }
 }
 if(i==512)e=ERROR_MORE_DATA;SetupDiDestroyDeviceInfoList(set);number("ROOT_COUNT",roots);number("CHILD_COUNT",children);if(!first)first=e?e:(roots!=1||children!=1?ERROR_NOT_FOUND:0);
 {typedef HANDLE(WINAPI *FIND)(const DWORD*,HANDLE*);typedef BOOL(WINAPI *CLOSE)(HANDLE);HMODULE m=LoadLibraryW(L"bthprops.cpl");HANDLE radio=NULL,find=NULL;DWORD param=sizeof(DWORD),error=0;
  if(!m)error=GetLastError();else{FIND f=(FIND)GetProcAddress(m,"BluetoothFindFirstRadio");CLOSE c=(CLOSE)GetProcAddress(m,"BluetoothFindRadioClose");if(!f||!c)error=ERROR_PROC_NOT_FOUND;else{SetLastError(0);find=f(&param,&radio);if(!find)error=GetLastError();else{if(!radio)error=ERROR_INVALID_HANDLE;if(!c(find)&&!error)error=GetLastError();}if(radio&&!CloseHandle(radio)&&!error)error=GetLastError();}FreeLibrary(m);}
  number("RADIO_FIRST_PRESENT",find&&radio?1:0);number("RADIO_ERROR",error);if(!first)first=error;
 }
 return first;
}
void mainCRTStartup(void){
 const WCHAR *s=GetCommandLineW();DWORD e,terminal=0,began=GetTickCount(),samples=0;
 if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}else while(*s&&*s!=' '&&*s!='\t')s++;while(*s==' '||*s=='\t')s++;
 if(lstrcmpW(s,L"--status")){number("RESULT",ERROR_INVALID_PARAMETER);ExitProcess(1);}
 do{
  number("SAMPLE_INDEX",samples++);e=observe(&terminal);
  /* BluetoothFindFirstRadio reports ERROR_NO_MORE_ITEMS until the software
   * radio interface arrives. Keep that observation distinct from success;
   * only CM terminal problems and other errors end the bounded wait early. */
  if(!e||terminal||(e!=ERROR_NOT_READY&&e!=ERROR_NOT_FOUND&&e!=ERROR_NO_MORE_ITEMS)||GetTickCount()-began>=35000u||samples>=141u)break;
  Sleep(250);
 }while(1);
 number("WAIT_ELAPSED_MS",GetTickCount()-began);number("WAIT_SAMPLES",samples);number("TERMINAL_CM_PROBLEM",terminal);number("RESULT",e);ExitProcess(e?1:0);
}
