/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * A real Win32/GDI workload. GPU use is established by the DOD UART traces,
 * not by this application returning successfully. No keyboard or camera. */
#include <windows.h>
#ifndef A04_GE8300_GDI_D0_TRANSITION
#define A04_GE8300_GDI_D0_TRANSITION 0
#endif
static DWORD pixels[800*1280];
static unsigned frame=1;
static int screen_width,screen_height;
static void ReportNumber(const char *label,DWORD value)
{
    char digits[12];DWORD count=0,written;const char *end=label;
    while(*end)end++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),label,(DWORD)(end-label),&written,NULL);
    do{digits[count++]=(char)('0'+value%10);value/=10;}while(value);
    while(count){count--;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),digits+count,1,&written,NULL);}
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),"\r\n",2,&written,NULL);
}
static LRESULT CALLBACK FrameProc(HWND w,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_PAINT) {
        PAINTSTRUCT ps;BITMAPINFO info;HDC dc;unsigned x,y;
        ZeroMemory(&info,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=screen_width;info.bmiHeader.biHeight=-screen_height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        for(y=0;y<(unsigned)screen_height;y++)for(x=0;x<(unsigned)screen_width;x++)
            pixels[y*screen_width+x]=0xff000000u|((x&255u)<<16)|((y&255u)<<8)|((x^y)&255u);
        if(frame>1)for(y=100;y<200;y++)for(x=100;x<200;x++)
            pixels[y*screen_width+x]=0xff000000u|((frame*0x17295bu)&0x00ffffffu);
        dc=BeginPaint(w,&ps);
        SetDIBitsToDevice(dc,0,0,screen_width,screen_height,0,0,0,screen_height,pixels,&info,DIB_RGB_COLORS);
        EndPaint(w,&ps);GdiFlush();return 0;
    }
    if(message==WM_TIMER) {
#if A04_GE8300_GDI_D0_TRANSITION
        if(frame==1)SetTimer(w,1,2000,NULL);
#endif
        if(frame>=8){DestroyWindow(w);return 0;}
        frame++;InvalidateRect(w,NULL,FALSE);UpdateWindow(w);return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcA(w,message,wp,lp);
}
void WinMainCRTStartup(void)
{
    WNDCLASSA wc;HWND window;MSG msg;HINSTANCE instance=GetModuleHandleA(NULL);
    /* Let the bounded startup check restore the present state before the
     * application begins its own real GDI updates. */
#if A04_GE8300_GDI_D0_TRANSITION
    /* The matched provider's diagnostic-only S0 idle timeout is 30 s.
     * Hide the console caret, hold a real GDI frame for 40 s, then repaint
     * to let KMDF resume the PDO and exercise retained CPU fallback. */
    Sleep(5000);
    ShowWindow(GetConsoleWindow(),SW_HIDE);
#else
    Sleep(45000);
#endif
    screen_width=GetSystemMetrics(SM_CXSCREEN);screen_height=GetSystemMetrics(SM_CYSCREEN);
    ReportNumber("GE8300_GDI_WIDTH=",screen_width);ReportNumber("GE8300_GDI_HEIGHT=",screen_height);
    if(screen_width<=0 || screen_height<=0 || screen_width>1280 || screen_height>1280 ||
       (unsigned)screen_width*(unsigned)screen_height>800u*1280u)ExitProcess(5);
    ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=FrameProc;wc.hInstance=instance;wc.lpszClassName="Ge8300GdiFrames";
    if(!RegisterClassA(&wc))ExitProcess(3);
    window=CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"Imagination PowerVR GE8300",WS_POPUP|WS_VISIBLE,
        0,0,screen_width,screen_height,NULL,NULL,instance,NULL);
    if(!window)ExitProcess(4);
    ShowWindow(window,SW_SHOW);UpdateWindow(window);
#if A04_GE8300_GDI_D0_TRANSITION
    SetTimer(window,1,40000,NULL);
#else
    SetTimer(window,1,2000,NULL);
#endif
    while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
#if A04_GE8300_GDI_D0_TRANSITION
    ShowWindow(GetConsoleWindow(),SW_SHOW);
#endif
    ExitProcess(frame==8?0:6);
}
