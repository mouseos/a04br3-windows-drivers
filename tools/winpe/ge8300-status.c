/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include "../../drivers/gpu/mt8167/ge8300_status.h"
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
    if(!DeviceIoControl(h,A04_GE8300_IOCTL_QUERY,NULL,0,&s,sizeof(s),&got,NULL) || got!=sizeof(s) || s.version!=3) {
        say("GE8300_IOCTL_ERROR="); hex(GetLastError()); say("\r\n"); CloseHandle(h); ExitProcess(2);
    }
    CloseHandle(h); say("GE8300_ATTEMPT="); hex(s.attempted); say(" BOOT="); hex(s.boot_status);
    say(" HEALTH="); hex(s.health_status); say(" SHUTDOWN="); hex(s.shutdown_status);
    say(" MMU="); hex(s.mmu_status);
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
    ExitProcess(s.attempted && s.boot_status==0 && s.health_status==0 && s.mmu_status==0 && s.shutdown_status==0 && !s.processor_released ? 0 : 3);
}
