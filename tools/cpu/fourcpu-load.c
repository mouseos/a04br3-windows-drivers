/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded four-CPU execution diagnostic with repeated CRC UART records. */
#include <windows.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#define CPU_COUNT 4u
#define TEST_MS 60000u
#define WAIT_SLICE 10000u
#define MAX_SLICES 9u
struct worker { unsigned cpu; volatile LONG state, observed, iterations, elapsed; };
static struct worker workers[CPU_COUNT];
static HANDLE start_event,uart;
static DWORD run_id;
static ULONG crc32(const unsigned char *p,unsigned n)
{ ULONG c=~0u;unsigned j;while(n--){c^=*p++;for(j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c; }
static void hex(char *s,DWORD v)
{ static const char d[]="0123456789ABCDEF";unsigned i;for(i=0;i<8;++i)s[i]=d[(v>>(28-4*i))&15u]; }
static int transmit(const char *s,DWORD size)
{
    DWORD count,start=GetTickCount(),repeat;
    for(repeat=0;repeat<3;++repeat) {
        while(!DeviceIoControl(uart,A04_UART_SCREEN_SEND,(void *)s,size,NULL,0,&count,NULL)) {
            if(GetLastError()!=ERROR_BUSY || (DWORD)(GetTickCount()-start)>=30000u)return 0;
            Sleep(5);
        }
    }
    return 1;
}
/* CRC covers the bytes through the last hex field, excluding the last space. */
static int record(DWORD seq,DWORD cpu,DWORD observed,DWORD loops,DWORD state,DWORD elapsed)
{
    static char line[80];DWORD fields[7],i,off=0;ULONG sum;
    line[off++]='A';line[off++]='0';line[off++]='4';line[off++]='C';line[off++]='U';line[off++]='1';line[off++]=' ';
    fields[0]=run_id;fields[1]=seq;fields[2]=cpu;fields[3]=observed;fields[4]=loops;fields[5]=state;fields[6]=elapsed;
    for(i=0;i<7;++i){hex(line+off,fields[i]);off+=8;line[off++]=' ';}
    sum=crc32((const unsigned char *)line,off-1);hex(line+off,sum);off+=8;line[off++]='\n';
    return transmit(line,off);
}
static DWORD WINAPI run_cpu(LPVOID param)
{
    struct worker *w=(struct worker *)param;ULONGLONG start;volatile DWORD value=0x12345678u;DWORD i,actual;
    if(!SetThreadAffinityMask(GetCurrentThread(),((DWORD_PTR)1)<<w->cpu)){InterlockedExchange(&w->state,0x10);return 1;}
    InterlockedExchange(&w->state,1);
    if(WaitForSingleObject(start_event,10000u)!=WAIT_OBJECT_0){InterlockedExchange(&w->state,0x11);return 2;}
    start=GetTickCount64();InterlockedExchange(&w->state,2);
    do {
        actual=GetCurrentProcessorNumber();InterlockedExchange(&w->observed,(LONG)actual);
        if(actual!=w->cpu){InterlockedExchange(&w->state,0x12);return 3;}
        for(i=0;i<65536u;++i)value=(value*1664525u)+1013904223u;
        InterlockedIncrement(&w->iterations);
    } while(GetTickCount64()-start<TEST_MS);
    (void)value;InterlockedExchange(&w->elapsed,(LONG)(DWORD)(GetTickCount64()-start));InterlockedExchange(&w->state,3);return 0;
}
static DWORD sample(volatile LONG *p){return (DWORD)InterlockedCompareExchange(p,0,0);}
static int snapshot(DWORD seq,DWORD elapsed)
{
    DWORD i,observed,loops,state,worker_elapsed;
    for(i=0;i<CPU_COUNT;++i){
        state=sample(&workers[i].state);observed=sample(&workers[i].observed);loops=sample(&workers[i].iterations);worker_elapsed=sample(&workers[i].elapsed);
        if(!record(seq,i,observed,loops,state,state==3?worker_elapsed:elapsed))return 0;
    }
    return 1;
}
void mainCRTStartup(void)
{
    SYSTEM_INFO info;DWORD_PTR process_mask,system_mask;HANDLE threads[CPU_COUNT];
    DWORD i,n=0,result=0,status,step,elapsed,start,mask=0,err;
    run_id=GetTickCount();start=run_id;
    do {uart=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(uart!=INVALID_HANDLE_VALUE)break;Sleep(50);} while((DWORD)(GetTickCount()-start)<30000u);
    if(uart==INVALID_HANDLE_VALUE)ExitProcess(4);
    if(!record(0,0xffffffffu,0,0,0,0)){CloseHandle(uart);ExitProcess(4);}
    GetSystemInfo(&info);
    if(info.dwNumberOfProcessors<CPU_COUNT || !GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask) || (process_mask&15u)!=15u || (system_mask&15u)!=15u){result=2;goto done;}
    start_event=CreateEventW(NULL,TRUE,FALSE,NULL);if(!start_event){result=3;goto done;}
    for(i=0;i<CPU_COUNT;++i){workers[i].cpu=i;workers[i].observed=-1;threads[i]=CreateThread(NULL,0,run_cpu,&workers[i],0,NULL);if(!threads[i])break;++n;}
    if(n!=CPU_COUNT){result=4;goto done;}
    if(!SetEvent(start_event)){result=5;goto done;}
    start=GetTickCount();
    for(step=1;step<=MAX_SLICES;++step){
        status=WaitForMultipleObjects(CPU_COUNT,threads,TRUE,WAIT_SLICE);elapsed=(DWORD)(GetTickCount()-start);
        if(!snapshot(step,elapsed)){result=7;goto done;}
        if(status==WAIT_OBJECT_0)break;
        if(status!=WAIT_TIMEOUT){result=8;goto done;}
    }
    if(step>MAX_SLICES){result=9;goto done;}
    for(i=0;i<CPU_COUNT;++i){err=sample(&workers[i].state);if(err==3 && sample(&workers[i].observed)==i && sample(&workers[i].iterations)>0 && sample(&workers[i].elapsed)>=TEST_MS)mask|=1u<<i;}
    if(mask!=15u)result=6;
done:
    elapsed=(DWORD)(GetTickCount()-start);
    if(!record(0xffffffffu,0xffffffffu,mask,n,result,elapsed))result=7;
    /* On failure, process exit closes handles and terminates any live workers. */
    CloseHandle(uart);ExitProcess(result);
}
