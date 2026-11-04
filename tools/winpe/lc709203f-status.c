/* SPDX-License-Identifier: MIT */
#include <windows.h>
static void say(const char *p)
{ DWORD n = 0, written; while (p[n]) ++n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), p, n, &written, NULL); }
static void hex(DWORD value)
{
    static const char digits[] = "0123456789ABCDEF";
    char out[9]; unsigned i;
    for (i = 0; i < 8; ++i) out[i] = digits[(value >> (28 - i * 4)) & 15];
    out[8] = 0; say(out);
}
void mainCRTStartup(void)
{
    SYSTEM_POWER_STATUS status;
    unsigned i;
    for (i = 0; i < 30; ++i) {
        if (GetSystemPowerStatus(&status) && status.BatteryLifePercent <= 100) break;
        Sleep(1000);
    }
    if (i == 30) { say("BATTERY_PERCENT_UNAVAILABLE\r\n"); ExitProcess(1); }
    say("BATTERY_PERCENT_HEX="); hex(status.BatteryLifePercent);
    say(" AC="); hex(status.ACLineStatus); say(" FLAG="); hex(status.BatteryFlag); say("\r\n");
    ExitProcess(0);
}
