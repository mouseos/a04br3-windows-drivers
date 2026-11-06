/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * A real Win32/GDI workload. GPU use is established by the DOD UART traces,
 * not by this application returning successfully. No keyboard or camera. */
#include <windows.h>
static DWORD pixels[800*1280];
static unsigned frame=1;
static LRESULT CALLBACK FrameProc(HWND w,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_PAINT) {
        PAINTSTRUCT ps;BITMAPINFO info;HDC dc;unsigned x,y;
        ZeroMemory(&info,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=800;info.bmiHeader.biHeight=-1280;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        for(y=0;y<1280;y++)for(x=0;x<800;x++)
            pixels[y*800+x]=0xff000000u|(((x+frame*29u)&255u)<<16)|
                (((y+frame*41u)&255u)<<8)|((x^y^(frame*73u))&255u);
        dc=BeginPaint(w,&ps);
        SetDIBitsToDevice(dc,0,0,800,1280,0,0,0,1280,pixels,&info,DIB_RGB_COLORS);
        EndPaint(w,&ps);GdiFlush();return 0;
    }
    if(message==WM_TIMER) {
        if(frame>=8){DestroyWindow(w);return 0;}
        frame++;InvalidateRect(w,NULL,FALSE);UpdateWindow(w);return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcA(w,message,wp,lp);
}
void WinMainCRTStartup(void)
{
    WNDCLASSA wc;HWND window;MSG msg;HINSTANCE instance=GetModuleHandleA(NULL);
    /* Startup diagnostic owns the present mutex until all sixteen frames and
     * restoration complete. This wait avoids counting them as GDI presents. */
    Sleep(45000);
    if(GetSystemMetrics(SM_CXSCREEN)!=800 || GetSystemMetrics(SM_CYSCREEN)!=1280)ExitProcess(5);
    ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=FrameProc;wc.hInstance=instance;wc.lpszClassName="Ge8300GdiFrames";
    if(!RegisterClassA(&wc))ExitProcess(3);
    window=CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"Imagination PowerVR GE8300",WS_POPUP|WS_VISIBLE,
        0,0,800,1280,NULL,NULL,instance,NULL);
    if(!window)ExitProcess(4);
    ShowWindow(window,SW_SHOW);UpdateWindow(window);SetTimer(window,1,2000,NULL);
    while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    ExitProcess(frame==8?0:6);
}
