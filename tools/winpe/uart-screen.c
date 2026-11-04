/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One-shot diagnostic capture of the Windows GDI desktop. */
#include <windows.h>
#include <stdint.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"
#define CHUNK 24u
#define MAX_BYTES (512u*1024u)
static void hex(char *s,uint32_t v,unsigned n) { static const char h[]="0123456789ABCDEF"; while(n) { s[--n]=h[v&15];v>>=4; } }
static uint32_t crc32(const unsigned char *p,unsigned n) { uint32_t c=~0u;unsigned j;while(n--) { c^=*p++;for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u & (0u-(c&1u))); }return ~c; }
static int screen_send(HANDLE h,const char *p,DWORD n,DWORD start) { DWORD got; while(GetTickCount()-start<900000u) { if(DeviceIoControl(h,A04_UART_SCREEN_SEND,(void*)p,n,NULL,0,&got,NULL)) return 1; if(GetLastError()!=ERROR_BUSY)return 0;Sleep(5); }return 0; }
static int send2(HANDLE h,const char *p,DWORD n,DWORD start) { return screen_send(h,p,n,start) && screen_send(h,p,n,start); }
int main(void)
{
 HDC screen=NULL,mem=NULL;HBITMAP bitmap=NULL,old=NULL;BITMAPINFO bi;void *pixels=NULL;
 HANDLE port=INVALID_HANDLE_VALUE;unsigned char *bits=NULL;unsigned w,h,stride,size,total,seq,x,y,n,i;DWORD start=GetTickCount(),id=start;char line[96];int rc=1;
 int wi=GetSystemMetrics(SM_CXSCREEN),hi=GetSystemMetrics(SM_CYSCREEN),full=0;
 const WCHAR *cmd=GetCommandLineW();while(*cmd){if(cmd[0]==L'-' && cmd[1]==L'f')full=1;cmd++;}
 if(wi<=0||hi<=0||wi>2048||hi>2048)goto done;w=full?(unsigned)wi:((unsigned)wi+1u)/2u;h=full?(unsigned)hi:((unsigned)hi+1u)/2u;stride=(w+7u)/8u;size=stride*h;if(!size||size>MAX_BYTES)goto done;
 bits=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);if(!bits)goto done;
 ZeroMemory(&bi,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=(LONG)w;bi.bmiHeader.biHeight=-(LONG)h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 screen=GetDC(NULL);if(!screen)goto done;mem=CreateCompatibleDC(screen);if(!mem)goto done;bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);if(!bitmap||!pixels)goto done;old=SelectObject(mem,bitmap);if(!old||old==(HBITMAP)HGDI_ERROR)goto done;
 if(!StretchBlt(mem,0,0,(int)w,(int)h,screen,0,0,wi,hi,SRCCOPY|CAPTUREBLT))goto done;
 GdiFlush();
 for(y=0;y<h;y++)for(x=0;x<w;x++){const unsigned char *p=(const unsigned char*)pixels+(y*w+x)*4u;unsigned lum=29u*p[0]+150u*p[1]+77u*p[2];if(lum>=32768u)bits[y*stride+x/8u]|=(unsigned char)(0x80u>>(x&7u));}
 port=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(port==INVALID_HANDLE_VALUE)goto done;
 total=(size+CHUNK-1)/CHUNK; if(total>65535)goto done;
 CopyMemory(line,"A04SC0 ",7);hex(line+7,id,8);line[15]=' ';hex(line+16,w,4);line[20]=' ';hex(line+21,h,4);line[25]=' ';hex(line+26,total,4);line[30]='\n';if(!send2(port,line,31,start))goto done;
 for(seq=0;seq<total;seq++){unsigned offset=seq*CHUNK;n=size-offset;if(n>CHUNK)n=CHUNK;CopyMemory(line,"A04SC1 ",7);hex(line+7,id,8);line[15]=' ';hex(line+16,seq,4);line[20]=' ';hex(line+21,n,2);line[23]=' ';for(i=0;i<n;i++)hex(line+24+2*i,bits[offset+i],2);line[24+2*n]=' ';hex(line+25+2*n,crc32(bits+offset,n),8);line[33+2*n]='\n';if(!send2(port,line,34+2*n,start))goto done;}
 CopyMemory(line,"A04SCE ",7);hex(line+7,id,8);line[15]=' ';hex(line+16,crc32(bits,size),8);line[24]='\n';if(!send2(port,line,25,start))goto done;rc=0;
done:
 if(port!=INVALID_HANDLE_VALUE)CloseHandle(port);if(old&&old!=(HBITMAP)HGDI_ERROR)SelectObject(mem,old);if(bitmap)DeleteObject(bitmap);if(mem)DeleteDC(mem);if(screen)ReleaseDC(NULL,screen);if(bits)HeapFree(GetProcessHeap(),0,bits);return rc;
}
void mainCRTStartup(void) { ExitProcess((UINT)main()); }
