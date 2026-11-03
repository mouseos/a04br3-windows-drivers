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
    ULONG status = 0, problem = 0, i;
    for (i=0; i<30; ++i) {
        result=CM_Locate_DevNodeW(&node,L"ACPI\\MOU0003\\0",CM_LOCATE_DEVNODE_NORMAL);
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
