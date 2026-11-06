#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <wchar.h>
#include "../../tools/winpe/usb-bqhr-uart.c"
static jmp_buf done;static unsigned result,mode,sends;static DWORD ticks;static char lines[9][96];
HANDLE CreateFileW(const wchar_t*n,DWORD a,DWORD s,void*x,DWORD o,DWORD f,HANDLE t){(void)a;(void)s;(void)x;(void)o;(void)f;(void)t;if(wcsstr(n,L"ScreenUart"))return (HANDLE)1;if(mode==2)return INVALID_HANDLE_VALUE;return (HANDLE)2;}
int DeviceIoControl(HANDLE h,DWORD c,void*in,DWORD il,void*out,DWORD ol,DWORD*n,void*x){unsigned i;(void)x;(void)in;(void)il;if(h==(HANDLE)2){A04_BQHR_RESPONSE*r=out;assert(c==A04_BQHR_GET&&ol==sizeof(*r));memset(r,0,sizeof(*r));r->size=sizeof(*r);r->version=1;r->result=mode?0:3;for(i=0;i<128;i++)r->record[i]=(unsigned char)i;*n=sizeof(*r);return 1;}assert(h==(HANDLE)1&&c==A04_UART_SCREEN_SEND&&il<96&&sends<9);memcpy(lines[sends],in,il);lines[sends++][il]=0;*n=0;return 1;}
int CloseHandle(HANDLE h){(void)h;return 1;}DWORD GetTickCount(void){return ticks;}DWORD GetLastError(void){return 5;}void Sleep(DWORD n){ticks+=n;}HANDLE GetStdHandle(DWORD x){(void)x;return (HANDLE)3;}int WriteFile(HANDLE h,const void*p,DWORD n,DWORD*w,void*x){(void)h;(void)p;(void)x;*w=n;return 1;}void ExitProcess(unsigned n){result=n;longjmp(done,1);}
int main(void){for(mode=0;mode<3;mode++){sends=ticks=0;memset(lines,0,sizeof(lines));if(!setjmp(done))mainCRTStartup();if(mode==0){assert(result==0&&sends==9);assert(strstr(lines[0]," 0 00000003 00000000"));assert(strstr(lines[1]," 1 03020100 07060504 0B0A0908 0F0E0D0C"));assert(strstr(lines[8]," 8 73727170 77767574 7B7A7978 7F7E7D7C"));}else if(mode==1){assert(result==3&&sends==1);}else assert(result==3&&sends==1&&strstr(lines[0],"FFFFFFFF"));}return 0;}
