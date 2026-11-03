/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* WinPE-only diagnostic: exactly one explicit digit 0..4 sends a down/up pair.
 * A failed release can leave the Windows button state down. */
#include <windows.h>
#include <setupapi.h>

static const GUID button_interface =
    {0x30ebfbf8, 0xdf5f, 0x4d4d, {0x9f,0xc5,0xa2,0x6c,0x7f,0xd1,0xdf,0x4a}};

static void say(const char *message)
{
    DWORD n = 0, ignored;
    while (message[n]) ++n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, n, &ignored, NULL);
}

void mainCRTStartup(void)
{
    HDEVINFO devices = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA iface;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = NULL;
    DWORD needed = 0, written = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    WCHAR *cmd = GetCommandLineW();
    BYTE button;
    int result = 1;

    if (*cmd == L'"') { ++cmd; while (*cmd && *cmd != L'"') ++cmd; if (*cmd) ++cmd; }
    else { while (*cmd && *cmd != L' ' && *cmd != L'\t') ++cmd; }
    while (*cmd == L' ' || *cmd == L'\t') ++cmd;
    if (cmd[0] < L'0' || cmd[0] > L'4' || cmd[1] != 0) {
        say("BUTTON_TEST requires exactly one button index 0..4\r\n"); ExitProcess(2);
    }
    button = (BYTE)(cmd[0] - L'0');
    devices = SetupDiGetClassDevsW(&button_interface, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) goto done;
    ZeroMemory(&iface, sizeof(iface)); iface.cbSize = sizeof(iface);
    if (!SetupDiEnumDeviceInterfaces(devices, NULL, &button_interface, 0, &iface)) goto done;
    SetupDiGetDeviceInterfaceDetailW(devices, &iface, NULL, 0, &needed, NULL);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed < sizeof(*detail)) goto done;
    detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, needed);
    if (!detail) goto done;
    detail->cbSize = sizeof(*detail);
    if (!SetupDiGetDeviceInterfaceDetailW(devices, &iface, detail, needed, NULL, NULL)) goto done;
    file = CreateFileW(detail->DevicePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    if (!WriteFile(file, &button, 1, &written, NULL) || written != 1) goto done;
    Sleep(100);
    if (!WriteFile(file, &button, 1, &written, NULL) || written != 1) {
        say("BUTTON_TEST release failed; button state may remain down\r\n"); goto done;
    }
    say("BUTTON_TEST down/up notifications accepted\r\n"); result = 0;
done:
    if (result) say("BUTTON_TEST failed\r\n");
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (detail) HeapFree(GetProcessHeap(), 0, detail);
    if (devices != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(devices);
    ExitProcess(result);
}
