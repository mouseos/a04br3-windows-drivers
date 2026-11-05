/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include "../../drivers/gpu/mt8167/ge8300_status.h"
#ifdef A04_GE8300_VENDOR_STAGE_STATUS
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
static int vendor_uart(const char *message)
{
    HANDLE port; DWORD n=0,sent,start=GetTickCount();
    while(message[n]) ++n;
    port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(port==INVALID_HANDLE_VALUE)return 0;
    while(!DeviceIoControl(port,A04_UART_SCREEN_SEND,(void *)message,n,NULL,0,&sent,NULL)) {
        if(GetLastError()!=ERROR_BUSY || GetTickCount()-start>10000u) {CloseHandle(port);return 0;}
        Sleep(10);
    }
    Sleep(1000);CloseHandle(port);return 1;
}
#endif
static void say(const char *p) { DWORD n=0,w; while(p[n])++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),p,n,&w,NULL); }
static void hex(DWORD n) { static const char d[]="0123456789ABCDEF"; char s[9]; unsigned i; for(i=0;i<8;++i)s[i]=d[(n>>(28-4*i))&15]; s[8]=0; say(s); }
void mainCRTStartup(void)
{
    HANDLE h=INVALID_HANDLE_VALUE; DWORD got=0; unsigned i; struct a04_ge8300_status s;
    for(i=0;i<45;++i) {
        h=CreateFileW(L"\\\\.\\A04GE8300",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
        if(h!=INVALID_HANDLE_VALUE) break;
        Sleep(1000);
    }
    if(h==INVALID_HANDLE_VALUE) { say("GE8300_OPEN_ERROR="); hex(GetLastError()); say("\r\n"); ExitProcess(1); }
#ifdef A04_GE8300_VENDOR_STAGE_STATUS
    {
        struct a04_ge8300_vendor_stage_status v;
        if(!DeviceIoControl(h,A04_GE8300_IOCTL_VENDOR_STAGE_QUERY,NULL,0,&v,sizeof(v),&got,NULL) ||
           got!=sizeof(v) || v.version!=1) {
            say("GE8300_VENDOR_STAGE_IOCTL_ERROR=");hex(GetLastError());say("\r\n");CloseHandle(h);ExitProcess(2);
        }
        CloseHandle(h);say("GE8300_VENDOR_STAGE_ATTEMPT=");hex(v.attempted);
        say(" STATUS=");hex(v.stage_status);say("\r\n");
        if(v.attempted && v.stage_status==0) ExitProcess(vendor_uart("A04GPU VENDOR_STAGE SUCCESS\n") ? 0 : 4);
        (void)vendor_uart("A04GPU VENDOR_STAGE FAILED\n");ExitProcess(3);
    }
#endif
    for(i=0;i<16;++i) {
        if(!DeviceIoControl(h,A04_GE8300_IOCTL_QUERY,NULL,0,&s,sizeof(s),&got,NULL) || got!=sizeof(s) || s.version!=5) {
            say("GE8300_IOCTL_ERROR="); hex(GetLastError()); say("\r\n"); CloseHandle(h); ExitProcess(2);
        }
#ifdef A04_GE8300_COPY_STATUS
        if(!s.copy_completed && !s.copy_failed && !s.queue_faulted && i<15) { Sleep(1000); continue; }
#endif
        break;
    }
    CloseHandle(h); say("GE8300_ATTEMPT="); hex(s.attempted); say(" BOOT="); hex(s.boot_status);
    say(" HEALTH="); hex(s.health_status); say(" SHUTDOWN="); hex(s.shutdown_status);
    say(" MMU="); hex(s.mmu_status);
    say(" QUEUE="); hex(s.queue_status);
    say("\r\nGE8300_BVNC="); hex(s.bvnc_high); hex(s.bvnc_low);
    say(" RELEASED="); hex(s.processor_released); say(" STARTED="); hex(s.firmware_started);
    say(" QUEUE_FAILED="); hex(s.queue_faulted); say("\r\n");
    say("GE8300_SYNC="); hex(s.sync_error); say(" Q_R="); hex(s.queue_read); say(" Q_W="); hex(s.queue_write);
    say(" RET="); hex(s.queue_return); say(" POWER_SYNC="); hex(s.power_sync); say("\r\n");
    say("GE8300_CONN_FW="); hex(s.connection_fw); say(" OS="); hex(s.connection_os);
    say(" PWR="); hex(s.fw_power_state); say(" EXEC="); hex(s.commands_executed); say("\r\n");
    say("GE8300_MTS_EN="); hex(s.mts_enable); say(" BG="); hex(s.mts_bg); say(" INT="); hex(s.mts_int);
    say(" COUNT="); hex(s.mts_count_high); hex(s.mts_count_low); say("\r\n");
    say("GE8300_TRACE_PTR="); hex(s.trace_pointer); say(" WORDS=");
    for(i=0;i<4;++i) { hex(s.trace_words[i]); say(" "); } say("\r\n");
    say("GE8300_COPY_COMPLETED="); hex(s.copy_completed); say(" FAILED="); hex(s.copy_failed); say("\r\n");
#ifdef A04_GE8300_COPY_STATUS
    ExitProcess(s.attempted && s.boot_status==0 && s.health_status==0 && s.mmu_status==0 &&
                s.queue_status==0 && s.processor_released && s.firmware_started && !s.queue_faulted &&
                s.copy_completed && !s.copy_failed ? 0 : 3);
#else
#ifdef A04_GE8300_SERVICE_STATUS
    ExitProcess(s.attempted && s.boot_status==0 && s.health_status==0 && s.mmu_status==0 &&
                s.queue_status==0 && s.processor_released && s.firmware_started && !s.queue_faulted ? 0 : 3);
#else
    ExitProcess(s.attempted && s.boot_status==0 && s.health_status==0 && s.mmu_status==0 && s.queue_status==0 && s.shutdown_status==0 && !s.processor_released ? 0 : 3);
#endif
#endif
}
