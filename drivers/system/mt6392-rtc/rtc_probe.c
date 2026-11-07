/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <windows.h>
#include <winioctl.h>
#include "rtc_ioctl.h"
static HANDLE output;
static void text(const char *s) { DWORD n=0,len=0;while(s[len])len++;WriteFile(output,s,len,&n,NULL); }
static void number(DWORD n) { char b[11];unsigned k=0;do {b[k++]=(char)('0'+n%10);n/=10;}while(n);while(k){char c=b[--k];DWORD w;WriteFile(output,&c,1,&w,NULL);} }
static void field(const char *name,DWORD n) {text(name);number(n);text("\r\n");}
void mainCRTStartup(void)
{
    HANDLE device;struct mt6392_rtc_sample sample;DWORD bytes=0,error=0;
    output=GetStdHandle(STD_OUTPUT_HANDLE);
    device=CreateFileW(L"\\\\.\\MT6392Rtc",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(device==INVALID_HANDLE_VALUE) {field("OPEN_ERROR=",GetLastError());ExitProcess(1);}
    ZeroMemory(&sample,sizeof(sample));
    if(!DeviceIoControl(device,IOCTL_MT6392_RTC_READ,NULL,0,&sample,sizeof(sample),&bytes,NULL))error=GetLastError();
    CloseHandle(device);field("READ_ERROR=",error);field("BYTES=",bytes);
    if(error || bytes!=sizeof(sample) || sample.size!=sizeof(sample) || sample.version!=MT6392_RTC_SAMPLE_VERSION)ExitProcess(1);
    field("YEAR=",sample.calendar.year);field("MONTH=",sample.calendar.month);field("DAY=",sample.calendar.day);
    field("HOUR=",sample.calendar.hour);field("MINUTE=",sample.calendar.min);field("SECOND=",sample.calendar.sec);
    field("WEEKDAY_SUNDAY_ZERO=",sample.calendar.weekday);field("RAW_WEEKDAY=",sample.raw.dow);
    text("RTC_CALENDAR_READ_ONLY=1\r\nOSCILLATOR_ACCURACY_VERIFIED=0\r\n");ExitProcess(0);
}
