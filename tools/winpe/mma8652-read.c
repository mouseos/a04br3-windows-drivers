/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only one-shot HID report diagnostic, scoped to ACPI MOU0010 ancestry.
 */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <hidpi.h>
static void say(const char *s) { DWORD n=0,w; while(s[n]) ++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,NULL); }
static void hex(ULONG n) { const char *d="0123456789ABCDEF"; char b[9]; int i; for(i=0;i<8;i++) b[i]=d[(n>>(28-4*i))&15]; b[8]=0; say(b); }
static int belongs(DEVINST node) {
 unsigned i,j; char id[512]; const char prefix[]="ACPI\\MOU0010\\";
 for(i=0;i<12;i++) {
  if(CM_Get_Device_IDA(node,id,sizeof(id),0)!=CR_SUCCESS) return 0;
  for(j=0;j<sizeof(prefix)-1 && id[j] && id[j]==prefix[j];++j) {}
  if(j==sizeof(prefix)-1) return 1;
  if(CM_Get_Parent(&node,node,0)!=CR_SUCCESS) return 0;
 }
 return 0;
}
void mainCRTStartup(void) {
 GUID guid; HDEVINFO set; SP_DEVICE_INTERFACE_DATA iface; SP_DEVINFO_DATA dev;
 BYTE detailbuf[2048], report[128]; PSP_DEVICE_INTERFACE_DETAIL_DATA_A detail=(void *)detailbuf;
 HANDLE selected=INVALID_HANDLE_VALUE, h, ev; unsigned i, found=0; DWORD need, got, err; OVERLAPPED ov;
 PHIDP_PREPARSED_DATA pp; HIDP_CAPS caps;
 HidD_GetHidGuid(&guid); set=SetupDiGetClassDevsA(&guid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE) { say("MMA_HID_ENUM_FAILED\r\n"); ExitProcess(1); }
 for(i=0;i<256;i++) {
  ZeroMemory(&iface,sizeof(iface)); iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,NULL,&guid,i,&iface)) break;
  ZeroMemory(&dev,sizeof(dev)); dev.cbSize=sizeof(dev); detail->cbSize=sizeof(*detail); need=0;
  if(!SetupDiGetDeviceInterfaceDetailA(set,&iface,detail,sizeof(detailbuf),&need,&dev) || !belongs(dev.DevInst)) continue;
  h=CreateFileA(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
  if(h==INVALID_HANDLE_VALUE) continue;
  pp=NULL; ZeroMemory(&caps,sizeof(caps));
  if(!HidD_GetPreparsedData(h,&pp) || HidP_GetCaps(pp,&caps)!=HIDP_STATUS_SUCCESS || caps.UsagePage!=0x20 || caps.Usage!=0x73 || caps.InputReportByteLength!=9) {
   if(pp) HidD_FreePreparsedData(pp); CloseHandle(h); continue;
  }
  HidD_FreePreparsedData(pp); ++found;
  if(found==1) selected=h; else CloseHandle(h);
 }
 SetupDiDestroyDeviceInfoList(set);
 if(found!=1) { if(selected!=INVALID_HANDLE_VALUE) CloseHandle(selected); say("MMA_HID_MATCH_COUNT="); hex(found); say("\r\n"); ExitProcess(2); }
 ev=CreateEventW(NULL,TRUE,FALSE,NULL); if(!ev) { CloseHandle(selected); ExitProcess(3); }
 ZeroMemory(&ov,sizeof(ov)); ov.hEvent=ev; ZeroMemory(report,sizeof(report)); got=0;
 if(!ReadFile(selected,report,9,&got,&ov)) {
  err=GetLastError(); if(err!=ERROR_IO_PENDING || WaitForSingleObject(ev,5000)!=WAIT_OBJECT_0 || !GetOverlappedResult(selected,&ov,&got,FALSE)) {
   CancelIo(selected); GetOverlappedResult(selected,&ov,&got,TRUE); CloseHandle(ev); CloseHandle(selected); say("MMA_HID_READ_FAILED\r\n"); ExitProcess(4);
  }
 }
 CloseHandle(ev); CloseHandle(selected);
 if(got!=9 || report[0]!=1 || report[1]!=2 || report[2]!=4) { say("MMA_HID_REPORT_INVALID\r\n"); ExitProcess(5); }
 say("MMA_HID_X_CENTIG_HEX="); hex((ULONG)(LONG)(SHORT)(report[3]|(report[4]<<8)));
 say(" Y="); hex((ULONG)(LONG)(SHORT)(report[5]|(report[6]<<8)));
 say(" Z="); hex((ULONG)(LONG)(SHORT)(report[7]|(report[8]<<8))); say("\r\n"); ExitProcess(0);
}
