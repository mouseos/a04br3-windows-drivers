/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read the volatile model-only firmware handoff in WinPE. */
#include <windows.h>
static void say(const char *s) {
    DWORD n=0, done=0; while (s[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&done,NULL);
}
void mainCRTStartup(void) {
    HANDLE token=NULL; TOKEN_PRIVILEGES privileges; LUID luid;
    unsigned char model[7]; DWORD n; unsigned i;
    static const char hex[]="0123456789ABCDEF";
    char out[]="EMMC_MODEL=...... MID=..\r\n";
    if (!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token) ||
        !LookupPrivilegeValueW(NULL,L"SeSystemEnvironmentPrivilege",&luid)) goto fail;
    privileges.PrivilegeCount=1; privileges.Privileges[0].Luid=luid;
    privileges.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(token,FALSE,&privileges,0,NULL,NULL) ||
        GetLastError()!=ERROR_SUCCESS) goto fail;
    n=GetFirmwareEnvironmentVariableW(L"A04EmmcModel",L"{689e5e54-214e-4df7-a871-8f31e1559a21}",model,sizeof(model));
    if (n!=sizeof(model) || !model[0] || model[0]==0xff) goto fail;
    for (i=0; i<6; ++i) {
        if (model[i+1] < 0x20 || model[i+1] > 0x7e) goto fail;
        out[11+i]=(char)model[i+1];
    }
    out[22]=hex[model[0]>>4]; out[23]=hex[model[0]&15];
    say(out); CloseHandle(token); ExitProcess(0);
fail:
    if (token) CloseHandle(token);
    say("EMMC_MODEL_READ_FAILED\r\n"); ExitProcess(1);
}
