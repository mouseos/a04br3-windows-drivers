/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * WinPE-only reset/worker lifetime probe. The target read is held for two
 * seconds in the diagnostic driver; it performs no direct writes. */
#include <windows.h>
#include <winioctl.h>
#include <ntddstor.h>
#include <ntddscsi.h>
#include <setupapi.h>
#ifndef A04_SCRATCH_LBA
#error Select the same scratch LBA as the diagnostic miniport
#endif
static HANDLE ready;
static const GUID storagePortGuid={0x2accfe60,0xc130,0x11d2,{0xb0,0x82,0x00,0xa0,0xc9,0x1e,0xfb,0x8b}};
static union { void *align; unsigned char bytes[4096]; } interfaceDetail;
static WCHAR hardwareIds[512];
static void say(const char *s) { DWORD n=0,w; while(s[n]) ++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
static void hex(DWORD value) {
    static const char digits[]="0123456789ABCDEF"; char text[11]; int i;
    text[0]='0'; text[1]='x'; text[10]=0;
    for(i=9;i>=2;--i) { text[i]=digits[value&15u]; value>>=4; }
    say(text); say("\r\n");
}
static void adapter_name(WCHAR out[32], UCHAR port) {
    static const WCHAR prefix[]=L"\\\\.\\Scsi"; WCHAR rev[3]; unsigned i=0,n=0;
    while(prefix[i]) { out[i]=prefix[i]; ++i; }
    do { rev[n++]=(WCHAR)(L'0'+port%10u); port=(UCHAR)(port/10u); } while(port);
    while(n) out[i++]=rev[--n]; out[i++]=L':'; out[i]=0;
}
static int matching_hardware_id(const WCHAR *ids, DWORD count) {
    static const WCHAR wanted[]=L"ACPI\\MOU0001"; DWORD i=0,j; WCHAR c;
    while(i<count && ids[i]) {
        j=0;
        while(i+j<count && wanted[j] && ids[i+j]) {
            c=ids[i+j]; if(c>=L'a' && c<=L'z') c=(WCHAR)(c-L'a'+L'A');
            if(c!=wanted[j]) break;
            ++j;
        }
        if(!wanted[j] && i+j<count && !ids[i+j]) return 1;
        while(i<count && ids[i]) ++i;
        ++i;
    }
    return 0;
}
/* Not every storage port publishes the legacy \\.\ScsiN: symbolic link.
 * Match the board's ACPI hardware ID before opening an enumerated interface. */
static HANDLE enumerate_adapter(void) {
    HDEVINFO set; SP_DEVICE_INTERFACE_DATA iface; SP_DEVINFO_DATA info;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail=(PSP_DEVICE_INTERFACE_DETAIL_DATA_W)interfaceDetail.bytes;
    HANDLE h=INVALID_HANDLE_VALUE; DWORD index,needed,type,bytes;
    set=SetupDiGetClassDevsW(&storagePortGuid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE) return h;
    for(index=0;;++index) {
        iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(set,NULL,&storagePortGuid,index,&iface)) break;
        info.cbSize=sizeof(info); detail->cbSize=sizeof(*detail); needed=0;
        if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,sizeof(interfaceDetail.bytes),&needed,&info)) continue;
        bytes=0; type=0;
        if(!SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_HARDWAREID,&type,
            (PBYTE)hardwareIds,sizeof(hardwareIds),&bytes) || type!=REG_MULTI_SZ ||
            !matching_hardware_id(hardwareIds,bytes/sizeof(WCHAR))) continue;
        h=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                      NULL,OPEN_EXISTING,0,NULL);
        if(h!=INVALID_HANDLE_VALUE) break;
    }
    SetupDiDestroyDeviceInfoList(set); return h;
}
static DWORD WINAPI reader(void *unused) {
    HANDLE h; LARGE_INTEGER off; DWORD n=0; unsigned char *buf; BOOL ok;
    (void)unused;
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
    buf=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(h==INVALID_HANDLE_VALUE || !buf) {
        if(buf) VirtualFree(buf,0,MEM_RELEASE);
        if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        SetEvent(ready); return 2;
    }
    off.QuadPart=((LONGLONG)(A04_SCRATCH_LBA+16u))<<9;
    if(!SetFilePointerEx(h,off,NULL,FILE_BEGIN)) {
        VirtualFree(buf,0,MEM_RELEASE); CloseHandle(h); SetEvent(ready); return 3;
    }
    SetEvent(ready);
    ok=ReadFile(h,buf,4096,&n,NULL);
    VirtualFree(buf,0,MEM_RELEASE); CloseHandle(h);
    return ok && n==4096 ? 0 : 1;
}
void mainCRTStartup(void) {
    HANDLE h=INVALID_HANDLE_VALUE, adapter=INVALID_HANDLE_VALUE, thread=NULL;
    STORAGE_BUS_RESET_REQUEST req; SCSI_ADDRESS address; WCHAR adapterPath[32];
    DWORD n=0,wait,readerResult=0; BOOL reset;
    ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                  NULL,OPEN_EXISTING,0,NULL);
    if(!ready || h==INVALID_HANDLE_VALUE) goto fail;
    if(!DeviceIoControl(h,IOCTL_SCSI_GET_ADDRESS,NULL,0,&address,sizeof(address),&n,NULL) || n<sizeof(address)) {
        say("MSDC_RESET_ADDRESS_ERROR="); hex(GetLastError()); goto fail;
    }
    adapter_name(adapterPath,address.PortNumber);
    adapter=CreateFileW(adapterPath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                        NULL,OPEN_EXISTING,0,NULL);
    if(adapter==INVALID_HANDLE_VALUE) adapter=enumerate_adapter();
    if(adapter==INVALID_HANDLE_VALUE) { say("MSDC_RESET_ADAPTER_ERROR="); hex(GetLastError()); goto fail; }
    thread=CreateThread(NULL,0,reader,NULL,0,NULL);
    if(!thread || WaitForSingleObject(ready,10000)!=WAIT_OBJECT_0) goto fail;
    Sleep(500);
    req.PathId=address.PathId;
    say("MSDC_RESET_IOCTL_STARTED\r\n");
    reset=DeviceIoControl(adapter,IOCTL_STORAGE_RESET_BUS,&req,sizeof(req),NULL,0,&n,NULL);
    if(!reset) { say("MSDC_RESET_IOCTL_ERROR="); hex(GetLastError()); }
    say(reset ? "MSDC_RESET_IOCTL_SUCCESS\r\n" : "MSDC_RESET_IOCTL_FAILED\r\n");
    wait=WaitForSingleObject(thread,10000);
    if(wait!=WAIT_OBJECT_0) goto fail;
    if(!GetExitCodeThread(thread,&readerResult)) goto fail;
    if(readerResult>1) goto fail;
    say(readerResult ? "MSDC_RESET_READ_FAILED\r\n" : "MSDC_RESET_READ_SUCCEEDED\r\n");
    say("MSDC_RESET_PROBE_RETURNED\r\n");
    CloseHandle(thread); CloseHandle(adapter); CloseHandle(h); CloseHandle(ready); ExitProcess(0);
fail:
    say("MSDC_RESET_PROBE_FAILED\r\n");
    if(thread) CloseHandle(thread);
    if(adapter!=INVALID_HANDLE_VALUE) CloseHandle(adapter);
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    if(ready) CloseHandle(ready);
    ExitProcess(1);
}
