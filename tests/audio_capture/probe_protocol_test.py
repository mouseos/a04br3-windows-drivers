"""実clientのrun/state本文をWin64 fixture化。PortCls解析由来の独立した契約を適用。"""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
current=(root/'tools/audio/mt8167-audio-capture-probe.c').read_text()
old=subprocess.check_output(['git','-C',str(root),'show','2d6231727e8ed72b6d7dca4148debc101ac5aeef:tools/audio/mt8167-audio-capture-probe.c'],text=True)
fixture=r'''
__declspec(dllimport) ULONG __stdcall RtlNtStatusToDosError(LONG Status);
static BYTE fake_buffer[16384];static DWORD mode,allocated,sets,runs,irq,position,pcm;
static KSSTATE current_state;
static int same(const GUID*a,const GUID*b){unsigned i;for(i=0;i<sizeof(*a);i++)if(((const BYTE*)a)[i]!=((const BYTE*)b)[i])return 0;return 1;}
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void line(const char*name,DWORD a,DWORD b,DWORD c){(void)a;(void)b;(void)c;if(eq(name,"pcm-aggregate"))pcm++;}
static HANDLE find_filter(void){return (HANDLE)1;}
static DWORD identifiers(HANDLE h,ULONG p,KSIDENTIFIER*i){(void)h;(void)p;ZeroMemory(i,sizeof(*i));return 0;}
static DWORD WINAPI fake_create(HANDLE h,PKSPIN_CONNECT p,ACCESS_MASK a,PHANDLE out){(void)h;(void)p;(void)a;*out=(HANDLE)2;return 0;}
static HMODULE fake_load(const WCHAR*p){(void)p;return (HMODULE)3;}
static FARPROC fake_get(HMODULE h,const char*p){(void)h;(void)p;return (FARPROC)fake_create;}
static BOOL fake_close(HANDLE h){(void)h;return TRUE;}
static BOOL fake_free(HMODULE h){(void)h;return TRUE;}
static void fake_sleep(DWORD n){(void)n;}
static BOOL control(HANDLE h,DWORD code,void*in,DWORD inbytes,void*out,DWORD outbytes,DWORD*got){
 KSPROPERTY*p=(KSPROPERTY*)in;(void)h;(void)code;*got=0;
 if(same(&p->Set,&KSPROPSETID_Connection)){
  KSSTATE value=*(KSSTATE*)out;sets++;
  if(p->Id!=KSPROPERTY_CONNECTION_STATE||p->Flags!=KSPROPERTY_TYPE_SET||inbytes!=sizeof(KSPROPERTY)||outbytes!=sizeof(KSSTATE))ExitProcess(61);
  /* Shipped RT8.1 handler 0x27b28: pin+0x4c must be set by buffer handler
   * before any non-STOP SetState, independently of the miniport. */
  if(value!=KSSTATE_STOP&&!allocated){SetLastError(RtlNtStatusToDosError((LONG)0xc0000184));return FALSE;}
  if(mode==2&&value==KSSTATE_PAUSE){SetLastError(ERROR_BAD_COMMAND);return FALSE;}
  current_state=value;if(value==KSSTATE_RUN)runs++;return TRUE;
 }
 if(same(&p->Set,&KSPROPSETID_RtAudio)&&p->Id==KSPROPERTY_RTAUDIO_BUFFER){
  KSRTAUDIO_BUFFER_PROPERTY*b=in;KSRTAUDIO_BUFFER*m=out;
  if(p->Flags!=KSPROPERTY_TYPE_GET||inbytes!=sizeof(*b)||outbytes!=sizeof(*m)||b->BaseAddress||b->RequestedBufferSize!=16384||current_state!=KSSTATE_STOP)ExitProcess(62);
  if(mode==1){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  ZeroMemory(m,sizeof(*m));m->BufferAddress=fake_buffer;m->ActualBufferSize=sizeof(fake_buffer);m->CallMemoryBarrier=TRUE;allocated=1;*got=sizeof(*m);return TRUE;
 }
 SetLastError(ERROR_NOT_SUPPORTED);return FALSE;
}
static DWORD snapshot(HANDLE f,struct a04_capture_status*s){(void)f;ZeroMemory(s,sizeof(*s));s->bytes=sizeof(*s);s->schema=1;s->running=current_state==KSSTATE_RUN;s->leased=allocated;s->valid=3;s->irq_events=++irq;s->position_bytes=position+=4;return 0;}
#define LoadLibraryW fake_load
#define GetProcAddress fake_get
#define CloseHandle fake_close
#define FreeLibrary fake_free
#define Sleep fake_sleep
'''
for name,source in [('old',old),('fixed',current)]:
 prefix=source[:source.index('static HANDLE logfile=')].replace('"../../drivers/audio/mt8167-afe/capture_status.h"','"capture_status.h"')
 state=source[source.index('static DWORD state('):source.index('static DWORD snapshot(')]
 run=source[source.index('static DWORD run(void)'):source.index('void mainCRTStartup(void)')]
 tail=r'''
static void reset(DWORD m){unsigned i;mode=m;allocated=sets=runs=irq=position=pcm=0;current_state=KSSTATE_STOP;for(i=0;i<sizeof(fake_buffer);i++)fake_buffer[i]=(BYTE)(i+1);}
void mainCRTStartup(void){DWORD result;
 if(RtlNtStatusToDosError((LONG)0xc0000184)!=ERROR_BAD_COMMAND)ExitProcess(63);
 reset(0);result=run();
'''
 if name=='old':tail+=' if(result!=ERROR_BAD_COMMAND||allocated||runs||pcm)ExitProcess(64);\n'
 else:tail+=''' if(result||!allocated||runs!=2||pcm!=1||current_state!=KSSTATE_STOP)ExitProcess(65);
 reset(1);result=run();if(result!=ERROR_INVALID_PARAMETER||allocated||sets||runs)ExitProcess(66);
 reset(2);result=run();if(result!=ERROR_BAD_COMMAND||!allocated||runs||pcm)ExitProcess(67);
'''
 tail+=' ExitProcess(0);\n}\n'
 (out/(name+'.c')).write_text(prefix+fixture+state+run+tail)
print('Generated exact old/fixed run and state bodies; no physical I/O')
