/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One-shot, fixed-target Windows RT 8.1 PnP installer. No CRT or shell. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"

static const WCHAR service_name[] = L"MT8167DeviceInstall";
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
static const WCHAR instance_id[] = L"ACPI\\MOU0006\\0";
static const WCHAR hardware_id[] = L"ACPI\\MOU0006";
#define TARGET_SERVICE L"MT8167GE8300"
#define TARGET_CLASS L"{4D36E97D-E325-11CE-BFC1-08002BE10318}"
#else
#define TARGET_SERVICE L"MT8167Display"
#define TARGET_CLASS L"{4D36E968-E325-11CE-BFC1-08002BE10318}"
static const WCHAR instance_id[] = L"ACPI\\MOU0007\\0";
static const WCHAR hardware_id[] = L"ACPI\\MOU0007";
#endif
static WCHAR system_dir[MAX_PATH], directory[MAX_PATH], package_dir[MAX_PATH];
static WCHAR executable[MAX_PATH], inf_path[MAX_PATH], log_path[MAX_PATH], marker_path[MAX_PATH];
static HANDLE logfile = INVALID_HANDLE_VALUE;
static BOOL log_ok = TRUE;
static const char *result_text = "FAILED";
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS service_status;
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
static DWORD gpu_service_exit=ERROR_GEN_FAILURE;
#endif
static HMODULE cm_module;
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
static HMODULE newdev_module;
#endif
typedef BOOL (WINAPI *update_fn)(HWND, LPCWSTR, LPCWSTR, DWORD, PBOOL);
typedef CONFIGRET (WINAPI *locate_fn)(PDEVINST, DEVINSTID_W, ULONG);
typedef CONFIGRET (WINAPI *status_fn)(PULONG, PULONG, DEVINST, ULONG);
typedef CONFIGRET (WINAPI *property_fn)(DEVINST, ULONG, PULONG, PVOID, PULONG, ULONG);
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
static update_fn update_driver;
#endif
static locate_fn locate_node;
static status_fn node_status;
static property_fn node_property;

/* Genuine byte operations for freestanding compiler-generated initializers. */
void *memset(void *dst, int value, size_t length)
{ unsigned char *p = dst; while (length--) *p++ = (unsigned char)value; return dst; }
void *memcpy(void *dst, const void *src, size_t length)
{ unsigned char *d = dst; const unsigned char *s = src; while (length--) *d++ = *s++; return dst; }

static void record(const char *s)
{
    DWORD count = 0, done = 0;
    while (s[count]) ++count;
    if (logfile == INVALID_HANDLE_VALUE || !WriteFile(logfile, s, count, &done, NULL) ||
        done != count || !FlushFileBuffers(logfile)) log_ok = FALSE;
}
static void number(const char *key, DWORD value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[96]; unsigned i, n = 0;
    while (*key && n < 70) text[n++] = *key++;
    text[n++] = '='; text[n++] = '0'; text[n++] = 'x';
    for (i = 0; i < 8; ++i) text[n++] = hex[(value >> (28 - 4 * i)) & 15];
    text[n++] = '\r'; text[n++] = '\n'; text[n] = 0; record(text);
}
static void timestamp(void)
{
    SYSTEMTIME t; GetSystemTime(&t);
    number("UTC_YEAR", t.wYear); number("UTC_MONTH", t.wMonth); number("UTC_DAY", t.wDay);
    number("UTC_HOUR", t.wHour); number("UTC_MINUTE", t.wMinute); number("UTC_SECOND", t.wSecond);
}
static DWORD check_directory(const WCHAR *path, BOOL create)
{
    DWORD attr;
    if (create && !CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return GetLastError();
    attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)) return ERROR_ACCESS_DENIED;
    return NO_ERROR;
}
static DWORD paths(void)
{
    DWORD n = GetSystemDirectoryW(system_dir, MAX_PATH), error;
    BY_HANDLE_FILE_INFORMATION info;
    if (!n || n >= MAX_PATH - 85) return ERROR_INSUFFICIENT_BUFFER;
    lstrcpyW(directory, system_dir); lstrcatW(directory,
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
        L"\\MT8167GpuUpdate"
#else
        L"\\MT8167DeviceInstall"
#endif
    );
    lstrcpyW(package_dir, directory); lstrcatW(package_dir,
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
        L"\\provider"
#else
        L"\\display"
#endif
    );
    lstrcpyW(executable, system_dir); lstrcatW(executable, L"\\MT8167DeviceInstall.exe");
    lstrcpyW(inf_path, package_dir); lstrcatW(inf_path,
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
        L"\\mt8167-ge8300.inf"
#else
        L"\\mt8167-display.inf"
#endif
    );
    lstrcpyW(log_path, directory); lstrcatW(log_path, L"\\install.log");
    lstrcpyW(marker_path, directory); lstrcatW(marker_path, L"\\attempted.bin");
    error = check_directory(directory, TRUE); if (error) return error;
    logfile = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, NULL);
    if (logfile == INVALID_HANDLE_VALUE) return GetLastError();
    if (!GetFileInformationByHandle(logfile, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(logfile); logfile = INVALID_HANDLE_VALUE; return ERROR_ACCESS_DENIED;
    }
    record("PROCESS_BEGIN\r\n"); timestamp();
    return log_ok ? NO_ERROR : ERROR_WRITE_FAULT;
}
static DWORD local_system(void)
{
    HANDLE token; BYTE buffer[256]; DWORD needed;
    BOOL system;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return GetLastError();
    if (!GetTokenInformation(token, TokenUser, buffer, sizeof buffer, &needed)) {
        DWORD error = GetLastError(); CloseHandle(token); return error;
    }
    system = IsWellKnownSid(((TOKEN_USER *)buffer)->User.Sid, WinLocalSystemSid);
    CloseHandle(token); return system ? NO_ERROR : ERROR_ACCESS_DENIED;
}
static DWORD disable_self(void)
{
    SC_HANDLE manager, service; DWORD error = NO_ERROR;
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager) return GetLastError();
    service = OpenServiceW(manager, service_name, SERVICE_CHANGE_CONFIG);
    if (!service) error = GetLastError();
    else {
        if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE,
                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL)) error = GetLastError();
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager); return error;
}
static DWORD claim_attempt(void)
{
    static const char message[] = "MT8167DeviceInstall ATTEMPTED\r\n";
    DWORD written = 0, error = NO_ERROR;
    HANDLE file = CreateFileW(marker_path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    if (!WriteFile(file, message, sizeof message - 1, &written, NULL) ||
        written != sizeof message - 1 || !FlushFileBuffers(file)) error = ERROR_WRITE_FAULT;
    CloseHandle(file); return error;
}
static HMODULE system_library(const WCHAR *name)
{
    WCHAR path[MAX_PATH]; lstrcpyW(path, system_dir); lstrcatW(path, L"\\"); lstrcatW(path, name);
    return LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
}
static DWORD load_apis(void)
{
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
    newdev_module = system_library(L"newdev.dll"); if (!newdev_module) return GetLastError();
#endif
    cm_module = system_library(L"cfgmgr32.dll"); if (!cm_module) return GetLastError();
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
    update_driver = (update_fn)GetProcAddress(newdev_module, "UpdateDriverForPlugAndPlayDevicesW");
#endif
    locate_node = (locate_fn)GetProcAddress(cm_module, "CM_Locate_DevNodeW");
    node_status = (status_fn)GetProcAddress(cm_module, "CM_Get_DevNode_Status");
    node_property = (property_fn)GetProcAddress(cm_module, "CM_Get_DevNode_Registry_PropertyW");
    return
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
        update_driver &&
#endif
        locate_node && node_status && node_property ? NO_ERROR : ERROR_PROC_NOT_FOUND;
}
static BOOL string_property(DEVINST node, ULONG property, const WCHAR *expected, BOOL nonempty)
{
    WCHAR value[256] = {0}; ULONG type = 0, bytes = sizeof value;
    CONFIGRET cr = node_property(node, property, &type, value, &bytes, 0);
    if (cr != CR_SUCCESS || type != REG_SZ || bytes < sizeof(WCHAR) || bytes > sizeof value ||
        value[bytes / sizeof(WCHAR) - 1] != 0) return FALSE;
    return nonempty ? value[0] != 0 : lstrcmpiW(value, expected) == 0;
}
static BOOL hardware_matches(DEVINST node)
{
    WCHAR value[512] = {0}; ULONG type = 0, bytes = sizeof value, i = 0, n;
    if (node_property(node, CM_DRP_HARDWAREID, &type, value, &bytes, 0) != CR_SUCCESS ||
        type != REG_MULTI_SZ || bytes < 4 || bytes > sizeof value || (bytes & 1)) return FALSE;
    n = bytes / sizeof(WCHAR);
    if (value[n - 1] || value[n - 2]) return FALSE;
    while (i < n && value[i]) {
        if (lstrcmpiW(value + i, hardware_id) == 0) return TRUE;
        while (i < n && value[i]) ++i;
        ++i;
    }
    return FALSE;
}
struct observation { CONFIGRET locate, status; ULONG flags, problem; BOOL hardware, driver, service, display_class; };
static struct observation observe(void)
{
    struct observation o = {0}; DEVINST node;
    o.locate = locate_node(&node, (DEVINSTID_W)instance_id, CM_LOCATE_DEVNODE_NORMAL);
    if (o.locate != CR_SUCCESS) { o.status = o.locate; return o; }
    o.status = node_status(&o.flags, &o.problem, node, 0);
    o.hardware = hardware_matches(node);
    o.driver = string_property(node, CM_DRP_DRIVER, NULL, TRUE);
    o.service = string_property(node, CM_DRP_SERVICE, TARGET_SERVICE, FALSE);
    o.display_class = string_property(node, CM_DRP_CLASSGUID, TARGET_CLASS, FALSE);
    return o;
}
static void log_observation(const char *phase, const struct observation *o)
{
    record(phase); record("\r\n");
    number("CM_LOCATE_CR", o->locate); number("CM_STATUS_CR", o->status);
    number("CM_STATUS", o->flags); number("CM_PROBLEM", o->problem);
    number("HARDWARE_MATCH", o->hardware); number("DRIVER_PRESENT", o->driver);
    number("SERVICE_MATCH", o->service); number("DISPLAY_CLASS_MATCH", o->display_class);
}
static BOOL bound(const struct observation *o)
{ return o->locate == CR_SUCCESS && o->status == CR_SUCCESS && o->hardware && o->driver && o->service && o->display_class; }
#if !defined(A04_GPU_PROVIDER_UPDATE) || !A04_GPU_PROVIDER_UPDATE
static BOOL started(const struct observation *o)
{ return bound(o) && (o->flags & DN_STARTED) && !(o->flags & DN_HAS_PROBLEM) && !o->problem; }
#endif
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
#include "gpu-provider-update.h"
#else
static DWORD hold_package(HANDLE files[3])
{
    static const WCHAR *names[] = {L"\\mt8167-display.inf", L"\\mt8167-display.sys", L"\\mt8167-display.cat"};
    DWORD i, error = check_directory(package_dir, FALSE);
    if (error) return error;
    for (i = 0; i < 3; ++i) {
        WCHAR path[MAX_PATH]; BY_HANDLE_FILE_INFORMATION info;
        lstrcpyW(path, package_dir); lstrcatW(path, names[i]);
        files[i] = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (files[i] == INVALID_HANDLE_VALUE) return GetLastError();
        if (!GetFileInformationByHandle(files[i], &info)) return GetLastError();
        if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            (!info.nFileSizeHigh && !info.nFileSizeLow)) return ERROR_INVALID_DATA;
    }
    return NO_ERROR;
}
static DWORD install_once(void)
{
    DWORD error, i; BOOL success = FALSE, reboot = FALSE;
    HANDLE files[3] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
    struct observation before, after;
    record("SERVICE_BEGIN\r\n");
    error = local_system(); number("LOCALSYSTEM_ERROR", error); if (error) goto done;
    /* Disable before claiming or attempting anything, even on failed preflight. */
    error = disable_self(); number("DISABLE_SERVICE_ERROR", error); if (error) goto done;
    error = claim_attempt(); number("CLAIM_ERROR", error);
    if (error) { record("NO_UPDATE_ATTEMPT\r\n"); goto done; }
    error = load_apis(); number("LOAD_API_ERROR", error); if (error) goto done;
    before = observe(); log_observation("BEFORE", &before);
    if (before.locate != CR_SUCCESS || before.status != CR_SUCCESS || !before.hardware) {
        error = ERROR_NOT_FOUND; goto after_snapshot;
    }
    error = hold_package(files); number("PACKAGE_ERROR", error); if (error) goto after_snapshot;
    if (!log_ok) { error = ERROR_WRITE_FAULT; goto after_snapshot; }
    record("UPDATE_BEGIN\r\n");
    if (!log_ok) { error = ERROR_WRITE_FAULT; goto after_snapshot; }
    SetLastError(NO_ERROR);
    success = update_driver(NULL, hardware_id, inf_path, INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE, &reboot);
    error = success ? NO_ERROR : GetLastError();
    if (!success && !error) error = ERROR_GEN_FAILURE;
    number("UPDATE_API_SUCCESS", success); number("UPDATE_WIN32_ERROR", error); number("UPDATE_REBOOT_REQUIRED", reboot);
    if (success) {
        /* One update call only. Bounded settling is not an installation retry. */
        for (i = 0; i < 30; ++i) {
            after = observe();
            if (started(&after) || reboot) break;
            Sleep(1000);
        }
    }
after_snapshot:
    after = observe(); log_observation("AFTER", &after);
    if (success) {
        if (started(&after)) { result_text = "BOUND_STARTED"; record("RESULT=BOUND_STARTED\r\n"); }
        else if (bound(&after) && reboot && (!after.problem || after.problem == CM_PROB_NEED_RESTART))
            { result_text = "BOUND_REBOOT_REQUIRED"; record("RESULT=BOUND_REBOOT_REQUIRED\r\n"); }
        else { error = ERROR_NOT_READY; result_text = "BIND_UNVERIFIED"; record("RESULT=BIND_UNVERIFIED\r\n"); }
        if (!error && reboot) error = ERROR_SUCCESS_REBOOT_REQUIRED;
    } else record("RESULT=UPDATE_NOT_SUCCEEDED\r\n");
done:
    for (i = 0; i < 3; ++i) if (files[i] != INVALID_HANDLE_VALUE) CloseHandle(files[i]);
    if (newdev_module) FreeLibrary(newdev_module);
    if (cm_module) FreeLibrary(cm_module);
    timestamp(); number("FINAL_WIN32_ERROR", error); record("SERVICE_END\r\n");
    return log_ok ? error : ERROR_WRITE_FAULT;
}
#endif
/* METHOD_BUFFERED queue contract: ASCII input, immediate enqueue or error.
 * One overlapped request; timeout never waits indefinitely for cancellation.
 * Storage/event remain alive until this one-shot process exits if pending. */
static void notify_end(DWORD installation_error)
{
    static OVERLAPPED overlapped;
    static char message[160];
    static const char hex[] = "0123456789ABCDEF";
    const char *prefix = "MT8167DeviceInstall SERVICE_END RESULT=";
    const char *result = result_text;
    DWORD n = 0, i, got = 0, error = NO_ERROR;
    HANDLE port; BOOL completed = TRUE;
    while (*prefix) message[n++] = *prefix++;
    while (*result) message[n++] = *result++;
    prefix = " WIN32=0x"; while (*prefix) message[n++] = *prefix++;
    for (i = 0; i < 8; ++i) message[n++] = hex[(installation_error >> (28 - 4 * i)) & 15];
    message[n++] = '\n';
    port = CreateFileW(L"\\\\.\\A04BR3ScreenUart", GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (port == INVALID_HANDLE_VALUE) { number("UART_NOTIFY_ERROR", GetLastError()); return; }
    overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent) { error = GetLastError(); goto finish; }
    if (!DeviceIoControl(port, A04_UART_SCREEN_SEND, message, n, NULL, 0, &got, &overlapped)) {
        error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            DWORD wait = WaitForSingleObject(overlapped.hEvent, 1000);
            if (wait == WAIT_OBJECT_0) error = GetOverlappedResult(port, &overlapped, &got, FALSE) ? NO_ERROR : GetLastError();
            else {
                error = wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
                completed = FALSE; (void)CancelIoEx(port, &overlapped);
            }
        }
    }
finish:
    CloseHandle(port);
    if (overlapped.hEvent && completed) CloseHandle(overlapped.hEvent);
    number("UART_NOTIFY_ERROR", error); /* enqueue acknowledgement, not wire delivery */
}
static void WINAPI control(DWORD code)
{ if (code == SERVICE_CONTROL_INTERROGATE && status_handle) SetServiceStatus(status_handle, &service_status); }
static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    DWORD error; (void)argc; (void)argv;
    status_handle = RegisterServiceCtrlHandlerW(service_name, control);
    if (!status_handle) { number("REGISTER_HANDLER_ERROR", GetLastError()); (void)disable_self(); return; }
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_RUNNING;
    /* No stop acceptance while a synchronous PnP install owns the package. */
    if (!SetServiceStatus(status_handle, &service_status)) { number("SCM_RUNNING_ERROR", GetLastError()); (void)disable_self(); return; }
    record("SCM_RUNNING\r\n"); error = install_once();
    notify_end(error);
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
    if(error==ERROR_SUCCESS_REBOOT_REQUIRED) {
        DWORD reboot_error=gpu_start_reboot_waiter();
        number("REBOOT_WAITER_START_ERROR",reboot_error);
        if(reboot_error)error=reboot_error;
    }
#endif
    service_status.dwCurrentState = SERVICE_STOPPED;
    service_status.dwWin32ExitCode = error;
    number("SCM_EXIT_WIN32", error);
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
    gpu_service_exit=log_ok?error:ERROR_WRITE_FAULT;
    service_status.dwWin32ExitCode=gpu_service_exit;
#endif
    if (!SetServiceStatus(status_handle, &service_status)) {
        DWORD status_error=GetLastError();number("SCM_STOP_ERROR",status_error);
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
        gpu_service_exit=status_error;
#endif
    }
}
static DWORD register_service(void)
{
    WCHAR module[MAX_PATH], command[MAX_PATH + 2]; DWORD n, error = NO_ERROR;
    SC_HANDLE manager, service;
    n = GetModuleFileNameW(NULL, module, MAX_PATH);
    if (!n || n >= MAX_PATH || lstrcmpiW(module, executable)) return ERROR_BAD_PATHNAME;
    if (GetFileAttributesW(marker_path) != INVALID_FILE_ATTRIBUTES) return ERROR_ALREADY_EXISTS;
    lstrcpyW(command, L"\""); lstrcatW(command, executable); lstrcatW(command, L"\"");
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE); if (!manager) return GetLastError();
    service = CreateServiceW(manager, service_name, service_name, SERVICE_QUERY_STATUS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, command,
        NULL, NULL, NULL, L"LocalSystem", NULL);
    if (!service) error = GetLastError(); else CloseServiceHandle(service);
    CloseServiceHandle(manager); return error;
}
static DWORD start_service(void)
{
    DWORD error = NO_ERROR; SC_HANDLE manager, service;
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT); if (!manager) return GetLastError();
    service = OpenServiceW(manager, service_name, SERVICE_START);
    if (!service) error = GetLastError();
    else { if (!StartServiceW(service, 0, NULL)) error = GetLastError(); CloseServiceHandle(service); }
    CloseServiceHandle(manager); return error;
}
static const WCHAR *arguments(void)
{
    const WCHAR *p = GetCommandLineW();
    if (*p == L'"') { ++p; while (*p && *p != L'"') ++p; if (*p) ++p; }
    else while (*p && *p != L' ' && *p != L'\t') ++p;
    while (*p == L' ' || *p == L'\t') ++p;
    return p;
}
void mainCRTStartup(void)
{
    DWORD error = paths(); const WCHAR *args = arguments();
    if (!error) {
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
        if (!lstrcmpW(args,L"--reboot-after-stop")) { error=gpu_finish_reboot(); }
        else
#endif
        if (!lstrcmpW(args, L"--install")) { error = register_service(); number("SCM_REGISTER_ERROR", error); }
        else if (!lstrcmpW(args, L"--start")) { error = start_service(); number("SCM_START_ERROR", error); }
        else if (*args) { error = ERROR_INVALID_PARAMETER; number("ARGUMENT_ERROR", error); }
        else {
            SERVICE_TABLE_ENTRYW table[2] = {{(LPWSTR)service_name, service_main}, {NULL, NULL}};
            if (!StartServiceCtrlDispatcherW(table)) { error = GetLastError(); number("DISPATCH_ERROR", error); }
#if defined(A04_GPU_PROVIDER_UPDATE) && A04_GPU_PROVIDER_UPDATE
            else error=gpu_service_exit;
#endif
        }
    }
    if (error && !*args) (void)disable_self();
    if (logfile != INVALID_HANDLE_VALUE) CloseHandle(logfile);
    ExitProcess(error);
}
