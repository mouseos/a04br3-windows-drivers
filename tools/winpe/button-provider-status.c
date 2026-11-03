/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only WinPE check of the specific ACPI provider's PnP state. */
#include <windows.h>
#include <cfgmgr32.h>

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
    ULONG status = 0, problem = 0, i, length;
    WCHAR ids[1024];
    for (i=0; i<30; ++i) {
        length=0;
        result=CM_Get_Device_ID_List_SizeW(&length,L"ACPI\\MOU0003",
                                           CM_GETIDLIST_FILTER_ENUMERATOR | CM_GETIDLIST_FILTER_PRESENT);
        if (result == CR_SUCCESS && length > 1 && length <= 1024) {
            ULONG end=0;
            result=CM_Get_Device_ID_ListW(L"ACPI\\MOU0003",ids,1024,
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
    say("BUTTON_PROVIDER_CM_RESULT="); hex(result);
    say(" STATUS="); hex(status); say(" PROBLEM="); hex(problem); say("\r\n");
    if (result == CR_SUCCESS && (status & DN_STARTED) && problem == 0) {
        say("BUTTON_PROVIDER_STARTED\r\n"); ExitProcess(0);
    }
    say("BUTTON_PROVIDER_NOT_STARTED\r\n"); ExitProcess(1);
}
