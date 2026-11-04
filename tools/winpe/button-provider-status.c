/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only WinPE check of the specific ACPI provider's PnP state. */
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#ifdef A04_DISPLAY_PROVIDER_STATUS
#include "../../drivers/gpu/mt8167/ge8300_status.h"
static void report_display(ULONG started)
{
    DWORD returned; HANDLE handle=CreateFileW(L"\\\\.\\A04GE8300",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(handle==INVALID_HANDLE_VALUE) return;
    DeviceIoControl(handle,A04_GE8300_IOCTL_REPORT_DISPLAY,&started,sizeof(started),NULL,0,&returned,NULL);
    CloseHandle(handle);
}
#endif

#ifdef A04_DISPLAY_PROVIDER_STATUS
#define PROVIDER_ID L"ACPI\\MOU0007"
#define PROVIDER_PREFIX "DISPLAY_PROVIDER"
#elif defined(A04_GE8300_PROVIDER_STATUS)
#define PROVIDER_ID L"ACPI\\MOU0006"
#define PROVIDER_PREFIX "GE8300_PROVIDER"
#elif defined(A04_TOPRGU_PROVIDER_STATUS)
#define PROVIDER_ID L"ACPI\\MOU0005"
#define PROVIDER_PREFIX "TOPRGU_PROVIDER"
#elif defined(A04_TOUCH_PROVIDER_STATUS)
#define PROVIDER_ID L"ACPI\\MOU0004"
#define PROVIDER_PREFIX "TOUCH_PROVIDER"
#else
#define PROVIDER_ID L"ACPI\\MOU0003"
#define PROVIDER_PREFIX "BUTTON_PROVIDER"
#endif

static void say(const char *text)
{
    DWORD done, length=0;
    while (text[length]) ++length;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, length, &done, NULL);
}
static void hex(ULONG value)
{
    static const char digits[] = "0123456789ABCDEF";
    char out[9]; unsigned i;
    for (i=0; i<8; ++i) out[i]=digits[(value >> (28-4*i)) & 15];
    out[8]=0; say(out);
}
void mainCRTStartup(void)
{
    DEVINST node = 0;
    CONFIGRET result = CR_FAILURE;
    ULONG status = 0, problem = 0, i, length, problemStatus = 0;
    DEVPROPTYPE propertyType = 0;
    CONFIGRET propertyResult = CR_FAILURE;
    WCHAR ids[1024];
    for (i=0; i<30; ++i) {
        length=0;
        result=CM_Get_Device_ID_List_SizeW(&length,PROVIDER_ID,
                                           CM_GETIDLIST_FILTER_ENUMERATOR | CM_GETIDLIST_FILTER_PRESENT);
        if (result == CR_SUCCESS && length > 1 && length <= 1024) {
            ULONG end=0;
            result=CM_Get_Device_ID_ListW(PROVIDER_ID,ids,1024,
                                         CM_GETIDLIST_FILTER_ENUMERATOR | CM_GETIDLIST_FILTER_PRESENT);
            if (result == CR_SUCCESS) {
                while (end < length && ids[end]) ++end;
                /* Refuse ambiguous multiple instances. */
                if (end + 1 >= length || ids[end+1] != 0) result=CR_FAILURE;
                else result=CM_Locate_DevNodeW(&node,ids,CM_LOCATE_DEVNODE_NORMAL);
            }
        } else if (result == CR_SUCCESS) result=CR_FAILURE;
        if (result == CR_SUCCESS) {
            result=CM_Get_DevNode_Status(&status,&problem,node,0);
            if (result == CR_SUCCESS && (status & DN_STARTED) && problem == 0) break;
        }
        Sleep(1000);
    }
    say(PROVIDER_PREFIX "_CM_RESULT="); hex(result);
    say(" STATUS="); hex(status); say(" PROBLEM="); hex(problem); say("\r\n");
    if (result == CR_SUCCESS) {
        length=sizeof(problemStatus);
        propertyResult=CM_Get_DevNode_PropertyW(node,&DEVPKEY_Device_ProblemStatus,
                         &propertyType,(PBYTE)&problemStatus,&length,0);
        say(PROVIDER_PREFIX "_PROPERTY_RESULT="); hex(propertyResult);
        say(" TYPE="); hex(propertyType); say(" NTSTATUS="); hex(problemStatus); say("\r\n");
    }
    if (result == CR_SUCCESS && (status & DN_STARTED) && problem == 0) {
#ifdef A04_DISPLAY_PROVIDER_STATUS
        report_display(1);
#endif
#ifdef A04_TOUCH_PROVIDER_STATUS
        DEVINST child;
        if (CM_Get_Child(&child, node, 0) == CR_SUCCESS) {
            for (i=0; i<16; ++i) {
                CHAR childId[512]; ULONG childStatus=0, childProblem=0;
                CONFIGRET childResult=CM_Get_Device_IDA(child, childId, sizeof(childId), 0);
                say("TOUCH_CHILD_ID=");
                if (childResult == CR_SUCCESS) say(childId); else say("UNAVAILABLE");
                childResult=CM_Get_DevNode_Status(&childStatus,&childProblem,child,0);
                say(" RESULT="); hex(childResult); say(" STATUS="); hex(childStatus);
                say(" PROBLEM="); hex(childProblem); say("\r\n");
                if (CM_Get_Sibling(&child,child,0) != CR_SUCCESS) break;
            }
        } else say("TOUCH_CHILD_NONE\r\n");
#endif
        say(PROVIDER_PREFIX "_STARTED\r\n"); ExitProcess(0);
    }
#ifdef A04_DISPLAY_PROVIDER_STATUS
    report_display(0);
#endif
    say(PROVIDER_PREFIX "_NOT_STARTED\r\n"); ExitProcess(1);
}
