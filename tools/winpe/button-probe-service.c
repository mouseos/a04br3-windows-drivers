/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One-shot LocalSystem diagnostic. It only enumerates the interface; it does
 * not synthesize input. The service logs a result and disables its own start. */
#include <windows.h>
#include <setupapi.h>

static const GUID button_interface =
    {0x30ebfbf8, 0xdf5f, 0x4d4d, {0x9f,0xc5,0xa2,0x6c,0x7f,0xd1,0xdf,0x4a}};
static SERVICE_STATUS_HANDLE service_handle;
static SERVICE_STATUS service_status;

static void log_result(const char *text)
{
    DWORD n = 0, done;
    HANDLE f = CreateFileW(L"C:\\Windows\\Temp\\a04-button-probe.txt", GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    while (text[n]) ++n;
    WriteFile(f, text, n, &done, NULL);
    FlushFileBuffers(f);
    CloseHandle(f);
}

static void WINAPI control(DWORD code)
{
    (void)code;
}

static void disable_self(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, L"A04ButtonProbe", SERVICE_CHANGE_CONFIG);
        if (svc) {
            ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                                 SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    DWORD i;
    int found = 0;
    (void)argc; (void)argv;
    service_handle = RegisterServiceCtrlHandlerW(L"A04ButtonProbe", control);
    if (!service_handle) return;
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(service_handle, &service_status);
    for (i = 0; i < 120; ++i) {
        HDEVINFO list = SetupDiGetClassDevsW(&button_interface, NULL, NULL,
                                             DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (list != INVALID_HANDLE_VALUE) {
            PSP_DEVICE_INTERFACE_DATA data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data));
            if (data) {
                data->cbSize = sizeof(*data);
                found = SetupDiEnumDeviceInterfaces(list, NULL, &button_interface, 0, data) != 0;
                HeapFree(GetProcessHeap(), 0, data);
            }
            SetupDiDestroyDeviceInfoList(list);
            if (found) break;
        }
        Sleep(1000);
    }
    log_result(found ? "A04_BUTTON_INTERFACE_PRESENT\r\n" : "A04_BUTTON_INTERFACE_NOT_FOUND\r\n");
    disable_self();
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = found ? NO_ERROR : ERROR_NOT_FOUND;
    SetServiceStatus(service_handle, &service_status);
}

void mainCRTStartup(void)
{
    SERVICE_TABLE_ENTRYW table[2];
    table[0].lpServiceName = L"A04ButtonProbe";
    table[0].lpServiceProc = service_main;
    table[1].lpServiceName = NULL;
    table[1].lpServiceProc = NULL;
    StartServiceCtrlDispatcherW(table);
    ExitProcess(0);
}
