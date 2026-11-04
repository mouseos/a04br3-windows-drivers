/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Displays the per-image machine-readable completion token generated offline.
 * Invoked only after successful mountvol and both completion messages.
 */
#include <windows.h>
static HBITMAP bitmap;
static LRESULT CALLBACK WindowProc(HWND w,UINT msg,WPARAM wp,LPARAM lp)
{
    PAINTSTRUCT ps; HDC dc, mem; BITMAP bm; RECT r; int x,y;
    switch (msg) {
    case WM_PAINT:
        dc=BeginPaint(w,&ps); GetClientRect(w,&r); FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
        mem=CreateCompatibleDC(dc); if (mem && bitmap && GetObjectA(bitmap,sizeof(bm),&bm)) {
            SelectObject(mem,bitmap); x=(r.right-bm.bmWidth)/2; y=(r.bottom-bm.bmHeight)/2;
            if (x>=0 && y>=0) BitBlt(dc,x,y,bm.bmWidth,bm.bmHeight,mem,0,0,SRCCOPY);
        }
        if (mem) DeleteDC(mem); EndPaint(w,&ps); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(w,msg,wp,lp);
}
void WinMainCRTStartup(void)
{
    WNDCLASSA wc; HWND window; MSG msg; HINSTANCE instance=GetModuleHandleA(NULL);
    bitmap=(HBITMAP)LoadImageA(NULL,"X:\\a04br3\\mma8652\\completion.bmp",IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);
    if (!bitmap) ExitProcess(2);
    ZeroMemory(&wc,sizeof(wc)); wc.lpfnWndProc=WindowProc; wc.hInstance=instance; wc.lpszClassName="A04MmaFinish";
    if (!RegisterClassA(&wc)) ExitProcess(3);
    window=CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"",WS_POPUP|WS_VISIBLE,0,0,
                            GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
    if (!window) ExitProcess(4);
    ShowWindow(window,SW_SHOW); UpdateWindow(window);
    while (GetMessageA(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    DeleteObject(bitmap); ExitProcess(0);
}
