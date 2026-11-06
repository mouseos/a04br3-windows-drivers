/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded ARM32 CPU-affinity load diagnostic. Does not modify the device. */
#include <windows.h>
#define CPU_COUNT 4u
#define TEST_MS 60000u
#define WAIT_MS 90000u
struct worker { unsigned cpu; DWORD result; DWORD observed; DWORD iterations; };
static struct worker workers[CPU_COUNT];
static HANDLE start_event;
static void print(const char *s)
{
    DWORD n=0,written; while (s[n]) ++n;
    (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&written,NULL);
}
static void hex(char *s,DWORD v)
{
    static const char digits[]="0123456789ABCDEF"; unsigned i;
    for(i=0;i<8;++i)s[i]=digits[(v>>(28-4*i))&15u];
}
static DWORD WINAPI run_cpu(LPVOID param)
{
    struct worker *w=(struct worker *)param;
    ULONGLONG start; volatile DWORD value=0x12345678u; DWORD i;
    if(!SetThreadAffinityMask(GetCurrentThread(),((DWORD_PTR)1)<<w->cpu)) {w->result=1;return 1;}
    if(WaitForSingleObject(start_event,10000u)!=WAIT_OBJECT_0) {w->result=2;return 2;}
    start=GetTickCount64();
    do {
        w->observed=GetCurrentProcessorNumber();
        if(w->observed!=w->cpu){w->result=3;return 3;}
        for(i=0;i<65536u;++i)value=(value*1664525u)+1013904223u;
        ++w->iterations;
    } while(GetTickCount64()-start<TEST_MS);
    (void)value;
    w->result=0;
    return 0;
}
void mainCRTStartup(void)
{
    SYSTEM_INFO info;DWORD_PTR process_mask,system_mask;HANDLE threads[CPU_COUNT];
    DWORD i,n=0,result=0,status;char line[]="A04CPU index=00000000 observed=00000000 loops=00000000 status=00000000\r\n";
    GetSystemInfo(&info);
    if(info.dwNumberOfProcessors<CPU_COUNT || !GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask) ||
       (process_mask&15u)!=15u || (system_mask&15u)!=15u){print("A04CPU AFFINITY_UNAVAILABLE\r\n");ExitProcess(2);}
    start_event=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!start_event){print("A04CPU EVENT_FAILED\r\n");ExitProcess(2);}
    for(i=0;i<CPU_COUNT;++i){workers[i].cpu=i;threads[i]=CreateThread(NULL,0,run_cpu,&workers[i],0,NULL);if(!threads[i])break;++n;}
    if(n!=CPU_COUNT){print("A04CPU THREAD_CREATE_FAILED\r\n");ExitProcess(2);}
    if(!SetEvent(start_event)){print("A04CPU EVENT_SET_FAILED\r\n");ExitProcess(2);}
    print("A04CPU LOAD_START duration_ms=60000\r\n");
    status=WaitForMultipleObjects(CPU_COUNT,threads,TRUE,WAIT_MS);
    if(status!=WAIT_OBJECT_0){print("A04CPU LOAD_WAIT_FAILED\r\n");ExitProcess(2);}
    for(i=0;i<CPU_COUNT;++i){
        hex(line+13,i);hex(line+31,workers[i].observed);hex(line+46,workers[i].iterations);hex(line+62,workers[i].result);
        print(line);if(workers[i].result || !workers[i].iterations)result=1;CloseHandle(threads[i]);
    }
    CloseHandle(start_event);
    print(result?"A04CPU LOAD_FAILED\r\n":"A04CPU LOAD_PASS_60S\r\n");
    ExitProcess(result);
}
