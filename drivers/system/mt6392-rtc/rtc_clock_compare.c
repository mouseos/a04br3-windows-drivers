/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed read-only same-process RTC / OS clock bracket. No command arguments,
 * clock setters, privileges, firmware variables, raw PMIC or user identities.
 * stdout must be redirected to a private log by the existing trusted runner. */
#include <windows.h>
#include <winioctl.h>
#include "rtc_ioctl.h"
void *memset(void *p,int value,size_t n){volatile unsigned char *b=p;while(n--)*b++=(unsigned char)value;return p;}
static HANDLE output;
static BOOL output_ok=TRUE;
static void text(const char *s){DWORD len=0,n=0;while(s[len])++len;if(!WriteFile(output,s,len,&n,NULL)||n!=len)output_ok=FALSE;}
static ULONGLONG div10(ULONGLONG n,unsigned *rem){ULONGLONG q=0;unsigned r=0,i;for(i=64;i;--i){r=(r<<1)|(unsigned)((n>>(i-1))&1);if(r>=10){r-=10;q|=(ULONGLONG)1<<(i-1);}}*rem=r;return q;}
static void number(ULONGLONG n){char b[21];unsigned k=0,d;do{n=div10(n,&d);b[k++]=(char)('0'+d);}while(n);while(k){char c[2];c[0]=b[--k];c[1]=0;text(c);}}
static void field(const char *s,ULONGLONG n){text(s);number(n);text("\r\n");}
static ULONGLONG ticks(const FILETIME *f){return ((ULONGLONG)f->dwHighDateTime<<32)|f->dwLowDateTime;}
static void calendar(const char *s,const SYSTEMTIME *t){text(s);number(t->wYear);text("-");number(t->wMonth);text("-");number(t->wDay);text(" ");number(t->wHour);text(":");number(t->wMinute);text(":");number(t->wSecond);text(".");number(t->wMilliseconds);text("\r\n");}
/* Arithmetic only: RTC components are encoded on FILETIME's calendar scale,
 * without asserting RTC timezone. delta is not a synchronization verdict. */
static void delta(const char *s,ULONGLONG rtc,ULONGLONG os){text(s);if(rtc<os){text("-");number(os-rtc);}else number(rtc-os);text("\r\n");}
void mainCRTStartup(void){
 HANDLE device;struct mt6392_rtc_sample sample;DWORD bytes=0,error=0,convert_error=0;
 FILETIME before,after,local_file,rtc_file;SYSTEMTIME utc_before,utc_after,local_before,local_after,rtc;
 ULONGLONG start,end;BOOL rb,ra,lb,la,rf,lf;DWORD result=1;
 output=GetStdHandle(STD_OUTPUT_HANDLE);
 text("SCHEMA=MT6392_OS_CLOCK_COMPARE_V1\r\nPRIVATE_LOG_REQUIRED=1\r\nRTC_TIMEZONE_KNOWN=0\r\nCLOCK_SET_REQUESTS=0\r\nSYNC_SUCCESS_CLAIM=0\r\n");
 device=CreateFileW(L"\\\\.\\MT6392Rtc",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE){field("OPEN_ERROR=",GetLastError());ExitProcess(1);}
 ZeroMemory(&sample,sizeof(sample));ZeroMemory(&rtc,sizeof(rtc));
 start=GetTickCount64();GetSystemTimeAsFileTime(&before);
 if(!DeviceIoControl(device,IOCTL_MT6392_RTC_READ,NULL,0,&sample,sizeof(sample),&bytes,NULL))error=GetLastError();
 GetSystemTimeAsFileTime(&after);end=GetTickCount64();CloseHandle(device);
 field("READ_ERROR=",error);field("READ_BYTES=",bytes);field("BRACKET_MONOTONIC_MS=",end>=start?end-start:0);
 field("MONOTONIC_ORDER_VALID=",end>=start);field("OS_UTC_BEFORE_FILETIME_100NS=",ticks(&before));field("OS_UTC_AFTER_FILETIME_100NS=",ticks(&after));
 field("OS_WALLCLOCK_ORDER_VALID=",ticks(&after)>=ticks(&before));
 rb=FileTimeToSystemTime(&before,&utc_before);if(!rb)convert_error=GetLastError();
 ra=FileTimeToSystemTime(&after,&utc_after);if(!ra&&!convert_error)convert_error=GetLastError();
 lb=rb&&SystemTimeToTzSpecificLocalTime(NULL,&utc_before,&local_before);if(rb&&!lb&&!convert_error)convert_error=GetLastError();
 la=ra&&SystemTimeToTzSpecificLocalTime(NULL,&utc_after,&local_after);if(ra&&!la&&!convert_error)convert_error=GetLastError();
 if(rb)calendar("OS_UTC_BEFORE=",&utc_before);if(ra)calendar("OS_UTC_AFTER=",&utc_after);
 if(lb)calendar("OS_LOCAL_BEFORE=",&local_before);if(la)calendar("OS_LOCAL_AFTER=",&local_after);
 field("OS_CALENDAR_CONVERSION_ERROR=",convert_error);
 if(!error&&bytes==sizeof(sample)&&sample.size==sizeof(sample)&&sample.version==MT6392_RTC_SAMPLE_VERSION){
  rtc.wYear=sample.calendar.year;rtc.wMonth=sample.calendar.month;rtc.wDay=sample.calendar.day;
  rtc.wHour=sample.calendar.hour;rtc.wMinute=sample.calendar.min;rtc.wSecond=sample.calendar.sec;
  calendar("RTC_LOCAL_UNSPECIFIED=",&rtc);rf=SystemTimeToFileTime(&rtc,&rtc_file);
  field("RTC_CALENDAR_VALID=",rf);
  if(rf){
   delta("RTC_COMPONENT_MINUS_OS_UTC_BEFORE_100NS=",ticks(&rtc_file),ticks(&before));
   delta("RTC_COMPONENT_MINUS_OS_UTC_AFTER_100NS=",ticks(&rtc_file),ticks(&after));
   lf=lb&&SystemTimeToFileTime(&local_before,&local_file);
   if(lf)delta("RTC_COMPONENT_MINUS_OS_LOCAL_BEFORE_100NS=",ticks(&rtc_file),ticks(&local_file));
   lf=la&&SystemTimeToFileTime(&local_after,&local_file);
   if(lf)delta("RTC_COMPONENT_MINUS_OS_LOCAL_AFTER_100NS=",ticks(&rtc_file),ticks(&local_file));
   if(rb&&ra&&lb&&la&&end>=start&&ticks(&after)>=ticks(&before))result=0;
  }
 }else text("RTC_SAMPLE_ABI_OR_READ_VALID=0\r\n");
 text("DELTA_IS_COMPONENT_ARITHMETIC_NOT_TIMEZONE_OR_SYNC_PROOF=1\r\n");
 field("COLLECTION_RC=",result);ExitProcess(output_ok?result:2);
}
