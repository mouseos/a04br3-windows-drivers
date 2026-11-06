#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <wchar.h>
#include "../../tools/winpe/usb-power-uart.c"
static jmp_buf done;static unsigned result,mode,sends,attempts,reads;static DWORD ticks,lastError;static char lines[3][96];
HANDLE CreateFileW(const wchar_t *name,DWORD access,DWORD share,void *s,DWORD how,DWORD flags,HANDLE t){(void)access;(void)share;(void)s;(void)how;(void)flags;(void)t;if(wcsstr(name,L"ScreenUart"))return (HANDLE)1;if(mode==2){lastError=5;return INVALID_HANDLE_VALUE;}return (HANDLE)2;}
int DeviceIoControl(HANDLE h,DWORD code,void *in,DWORD ins,void *out,DWORD outs,DWORD *n,void *ov){(void)ov;(void)ins;
 if(h==(HANDLE)2){A04_USB_OBSERVATION *v=out;assert(code==A04_USB_OBSERVATION_GET && !in && outs==sizeof(*v));reads++;memset(v,0,sizeof(*v));v->size=sizeof(*v);v->version=mode==1?99:1;v->dir1=1;v->dout1=2;v->din1=3;v->mode21_22=4;v->dir2=5;v->dout2=6;v->din2=7;v->mode34=8;v->mode41=9;*n=sizeof(*v);return 1;}
 assert(h==(HANDLE)1 && code==A04_UART_SCREEN_SEND && ins<96);attempts++;if(attempts==1){lastError=ERROR_BUSY;return 0;}
 assert(sends<3);memcpy(lines[sends],in,ins);lines[sends][ins]=0;sends++;*n=0;return 1;}
int CloseHandle(HANDLE h){(void)h;return 1;}DWORD GetTickCount(void){return ticks;}DWORD GetLastError(void){return lastError;}void Sleep(DWORD n){ticks+=n;}
HANDLE GetStdHandle(DWORD x){(void)x;return (HANDLE)3;}int WriteFile(HANDLE h,const void *p,DWORD n,DWORD *w,void *x){(void)h;(void)p;(void)x;*w=n;return 1;}
void ExitProcess(unsigned n){result=n;longjmp(done,1);}
int main(void){for(mode=0;mode<3;mode++){sends=attempts=reads=ticks=lastError=0;memset(lines,0,sizeof(lines));if(!setjmp(done))mainCRTStartup();
 if(!mode){assert(result==0 && reads==1 && sends==3 && attempts==4);assert(strstr(lines[0]," 1 00000001 00000002 00000003 00000004\n"));assert(strstr(lines[1]," 2 00000005 00000006 00000007 00000008\n"));assert(strstr(lines[2]," 3 00000009\n"));}
 else {assert(result==3 && sends==1 && strstr(lines[0],"A04USB ERROR"));assert(reads==(mode==1));}}
 return 0;}
