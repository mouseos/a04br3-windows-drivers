/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * CRT-free, single-file WinPE staging publication. Never opens dest for write.
 * Exit codes and reference-only command syntax are documented in the test README.
 */
#include <windows.h>
#include "../../drivers/system/mt8167-toprgu/uart_screen.h"

#ifndef A04_ATOMIC_REPLACE_REFERENCE
#define A04_ATOMIC_REPLACE_REFERENCE 0
#endif
#if A04_ATOMIC_REPLACE_REFERENCE != 0 && A04_ATOMIC_REPLACE_REFERENCE != 1
#error invalid reference variant
#endif

#define LIMIT_BYTES (64u * 1024u * 1024u)
#define START_LIMIT 1680000u
#define PUBLISH_LIMIT 1740000u
#define PATH_CAP 260u
static WCHAR command[2048], source[PATH_CAP], dest[PATH_CAP], temp[PATH_CAP];
static BYTE copybuf[65536], comparebuf[65536];
/* No FILE_SHARE_DELETE: keep every opened ancestor from being renamed away. */
static HANDLE parents[PATH_CAP * 2];
static DWORD parent_count;
static int parse_failed;
#if A04_ATOMIC_REPLACE_REFERENCE
static const char marker[]="A04_ATOMIC_STAGE_V1:TESTCLOCK:REFERENCE";
#else
static const char marker[]="A04_ATOMIC_STAGE_V1:LEASE_QUERY:PRODUCTION";
#endif
static void announce(void)
{
    DWORD sent;
    (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),marker,sizeof(marker)-1,&sent,NULL);
    (void)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),"\r\n",2,&sent,NULL);
}
#if A04_ATOMIC_REPLACE_REFERENCE
static ULONGLONG start_clock, publish_clock;
static int fault_before_publish;
#endif

static DWORD length(const WCHAR *s)
{
    DWORD n=0;
    while (s[n]) ++n;
    return n;
}
static WCHAR upper(WCHAR c)
{
    return c>=L'a' && c<=L'z' ? (WCHAR)(c-L'a'+L'A') : c;
}
static int equal(const WCHAR *a, const WCHAR *b)
{
    while (*a && upper(*a)==upper(*b)) { ++a; ++b; }
    return !*a && !*b;
}
static int prefix(const WCHAR *s, const WCHAR *p)
{
    while (*p) if (upper(*s++)!=upper(*p++)) return 0;
    return 1;
}
/* Deliberately small grammar: whole quoted tokens or unquoted tokens; no
 * embedded quotes, escape interpretation, shell, environment expansion or CRT.
 */
static WCHAR *argument(WCHAR **cursor)
{
    WCHAR *p=*cursor, *s;
    while (*p==L' ' || *p==L'\t') ++p;
    if (!*p) { *cursor=p; return NULL; }
    if (*p==L'"') {
        s=++p;
        while (*p && *p!=L'"') ++p;
        if (!*p) { parse_failed=1; return NULL; }
        *p++=0;
        if (*p && *p!=L' ' && *p!=L'\t') { parse_failed=1; return NULL; }
    } else {
        s=p;
        while (*p && *p!=L' ' && *p!=L'\t') {
            if (*p==L'"') { parse_failed=1; return NULL; }
            ++p;
        }
        if (*p) *p++=0;
    }
    *cursor=p;
    if (!*s) { parse_failed=1; return NULL; }
    return s;
}
/* Reject Win32 aliases/ADS before GetFullPathName; allow dot navigation only
 * when normalization still lands under the admitted prefix. */
static int spelling(const WCHAR *s)
{
    DWORD i;
    if (!s[0] || s[1]!=L':' || s[2]!=L'\\') return 0;
    for (i=2;s[i];++i) {
        WCHAR c=s[i];
        if (c<L' ' || c==L':' || c==L'/' || c==L'"' || c==L'<' ||
            c==L'>' || c==L'|' || c==L'?' || c==L'*') return 0;
    }
    return i<PATH_CAP;
}
static int component(const WCHAR *p, DWORD n)
{
    DWORD k=0;
    if (!n || p[n-1]==L'.' || p[n-1]==L' ') return 0;
    while (k<n && p[k]!=L'.') ++k;
    if (k && p[k-1]==L' ') return 0;
    if ((k==6 && prefix(p,L"CONIN$")) || (k==7 && prefix(p,L"CONOUT$")) ||
        (k==6 && prefix(p,L"CLOCK$"))) return 0;
    if (k==3 && ((upper(p[0])==L'C' && upper(p[1])==L'O' && upper(p[2])==L'N') ||
                  (upper(p[0])==L'P' && upper(p[1])==L'R' && upper(p[2])==L'N') ||
                  (upper(p[0])==L'A' && upper(p[1])==L'U' && upper(p[2])==L'X') ||
                  (upper(p[0])==L'N' && upper(p[1])==L'U' && upper(p[2])==L'L'))) return 0;
    if (k==4 && ((upper(p[0])==L'C' && upper(p[1])==L'O' && upper(p[2])==L'M') ||
                (upper(p[0])==L'L' && upper(p[1])==L'P' && upper(p[2])==L'T')) &&
        ((p[3]>=L'0' && p[3]<=L'9') || p[3]==0xb9 || p[3]==0xb2 || p[3]==0xb3)) return 0;
    return 1;
}
static int normalize(const WCHAR *raw, WCHAR *out, const WCHAR *allowed)
{
    DWORD n, i, start=3;
    if (!spelling(raw)) return 0;
    for (i=3;;++i) if (!raw[i] || raw[i]==L'\\') {
        DWORD count=i-start;
        int navigation=(count==1 && raw[start]==L'.') ||
            (count==2 && raw[start]==L'.' && raw[start+1]==L'.');
        if (!navigation && !component(raw+start,count)) return 0;
        start=i+1;
        if (!raw[i]) break;
    }
    start=3;
    n=GetFullPathNameW(raw,PATH_CAP,out,NULL);
    if (!n || n>=PATH_CAP || !prefix(out,allowed) || n<=length(allowed)) return 0;
    for (i=3;i<=n;++i) if (!out[i] || out[i]==L'\\') {
        if (!component(out+start,i-start)) return 0;
        start=i+1;
    }
    return 1;
}
static int information(HANDLE h, int directory, DWORD *serial)
{
    BY_HANDLE_FILE_INFORMATION info;
    if (h==INVALID_HANDLE_VALUE || GetFileType(h)!=FILE_TYPE_DISK ||
        !GetFileInformationByHandle(h,&info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        !!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=directory) return 0;
    if (serial) *serial=info.dwVolumeSerialNumber;
    return 1;
}
static int pin_parents(WCHAR *path, DWORD *volume)
{
    DWORD i, serial=0, root_serial=0;
    for (i=3;;++i) {
        WCHAR saved=path[i];
        HANDLE h;
        if (i!=3 && saved!=L'\\') { if (!saved) break; else continue; }
        /* C:\ stays intact; other parent separators are temporarily removed. */
        if (i!=3) path[i]=0;
        else { saved=path[3];path[3]=0; }
        h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
                      NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        path[i]=saved;
        if (parent_count>=PATH_CAP*2 || !information(h,1,&serial)) {
            if (h!=INVALID_HANDLE_VALUE) CloseHandle(h);
            return 0;
        }
        parents[parent_count++]=h;
        if (i==3) root_serial=serial;
        else if (serial!=root_serial) return 0;
    }
    if (volume) *volume=root_serial;
    return 1;
}
static int target_ok(void)
{
    HANDLE h=CreateFileW(dest,FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,NULL);
    int ok;
    if (h==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_FILE_NOT_FOUND;
    ok=information(h,0,NULL);
    if (!CloseHandle(h)) return 0;
    return ok;
}
static ULONGLONG clock_ms(int publishing)
{
#if A04_ATOMIC_REPLACE_REFERENCE
    return publishing ? publish_clock : start_clock;
#else
    (void)publishing;
    return GetTickCount64();
#endif
}
/* active==1 is the driver's proof of running/owned-four/enabled WDT.
 * No cached proof: open and query again for each publication attempt. */
static int lease_ready(int publishing)
{
    ULONGLONG ticks;
#if A04_ATOMIC_REPLACE_REFERENCE
    ticks=clock_ms(publishing)*10000u;
#else
    HANDLE device;
    struct a04_winpe_lease_status status;
    DWORD bytes, error;
    ULONGLONG began=GetTickCount64();
    device=CreateFileW(L"\\\\.\\A04BR3ScreenUart",GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if (device==INVALID_HANDLE_VALUE) return 0;
    for (;;) {
        bytes=0;
        if (DeviceIoControl(device,A04_UART_WINPE_LEASE_QUERY,NULL,0,
                            &status,sizeof(status),&bytes,NULL)) break;
        error=GetLastError();
        if (error!=ERROR_BUSY || GetTickCount64()-began>=1000u) {
            CloseHandle(device);return 0;
        }
        Sleep(20);
        if (GetTickCount64()-began>=1000u) { CloseHandle(device);return 0; }
    }
    if (!CloseHandle(device) || bytes!=sizeof(status) ||
        status.magic!=A04_WINPE_LEASE_STATUS_MAGIC || status.seconds!=1800u ||
        status.active!=1u) return 0;
    ticks=((ULONGLONG)status.elapsed_high<<32)|status.elapsed_low;
#endif
    return ticks<(ULONGLONG)(publishing ? PUBLISH_LIMIT : START_LIMIT)*10000u;
}
static DWORD start_status(void)
{
    if (clock_ms(0)>=START_LIMIT) return 3;
    if (!lease_ready(0)) return 11;
    /* A busy-query retry must not carry us past the uptime start boundary. */
    return clock_ms(0)>=START_LIMIT ? 3 : 0;
}
#if A04_ATOMIC_REPLACE_REFERENCE
static int number(const WCHAR *s, ULONGLONG *out)
{
    ULONGLONG v=0;
    if (!s || !*s) return 0;
    while (*s) {
        if (*s<L'0' || *s>L'9') return 0;
        v=v*10+(unsigned)(*s++-L'0');
        if (v>0xffffffffu) return 0;
    }
    *out=v;return 1;
}
#endif
void mainCRTStartup(void)
{
    const WCHAR *raw=GetCommandLineW();
    WCHAR *cursor, *a, *b;
    DWORD i, n, got, written, other, serial, dest_serial, total=0, rc=2;
    HANDLE src=INVALID_HANDLE_VALUE, tmp=INVALID_HANDLE_VALUE;
    LARGE_INTEGER size, zero;
    WCHAR fs[32];
    announce();
    for (n=0;n<2047 && raw[n];++n) command[n]=raw[n];
    if (raw[n]) goto done;
    command[n]=0;cursor=command;
    if (!argument(&cursor) || !(a=argument(&cursor))) goto done;
#if !A04_ATOMIC_REPLACE_REFERENCE
    if (equal(a,L"--check-lease")) {
        if (argument(&cursor) || parse_failed) goto done;
        /* No path/volume lookup, source open, temp creation or publication.
         * Only stdout marker and the read-only device query are performed. */
        rc=start_status();
        goto done;
    }
#endif
    if (!(b=argument(&cursor))) goto done;
#if A04_ATOMIC_REPLACE_REFERENCE
    {
        WCHAR *flag=argument(&cursor), *first=argument(&cursor), *last=argument(&cursor);
        if (!flag || !equal(flag,L"--testclock") || !number(first,&start_clock) ||
            !number(last,&publish_clock) || publish_clock<start_clock) goto done;
        flag=argument(&cursor);
        if (flag) {
            if (!equal(flag,L"--fault-before-publish")) goto done;
            fault_before_publish=1;
        }
    }
    if (!normalize(a,source,L"C:\\mt8167-resume235-native-43253681009\\atomic-reference\\") ||
        !normalize(b,dest,L"C:\\mt8167-resume235-native-43253681009\\atomic-reference\\")) goto done;
#else
    if (!normalize(a,source,L"X:\\a04br3\\payload\\") ||
        !normalize(b,dest,L"C:\\Windows\\System32\\")) goto done;
#endif
    if (argument(&cursor) || parse_failed || equal(source,dest)) goto done;
    n=length(dest);
    if (n+length(L".a04-stage.tmp")>=PATH_CAP) goto done;
    for (i=0;i<n;++i) temp[i]=dest[i];
    for (i=0;i<=length(L".a04-stage.tmp");++i) temp[n+i]=L".a04-stage.tmp"[i];
    rc=start_status();
    if (rc) goto done;
    rc=4;
    if (GetDriveTypeW(L"C:\\")!=DRIVE_FIXED ||
        !GetVolumeInformationW(L"C:\\",NULL,0,NULL,NULL,NULL,fs,32) || !equal(fs,L"NTFS") ||
        !pin_parents(source,NULL) || !pin_parents(dest,&dest_serial) || !target_ok()) goto done;
    src=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if (!information(src,0,NULL) || !GetFileSizeEx(src,&size) ||
        size.QuadPart<0 || size.QuadPart>LIMIT_BYTES) goto done;
    /* Recheck after potentially slow path validation, immediately before copy. */
    rc=start_status();
    if (rc) goto done;
    rc=5;
    tmp=CreateFileW(temp,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if (!information(tmp,0,&serial) || serial!=dest_serial) goto done;
    rc=6;
    for (;;) {
        if (!ReadFile(src,copybuf,sizeof(copybuf),&got,NULL)) goto done;
        if (!got) break;
        if (got>LIMIT_BYTES-total) goto done;
        total+=got;
        for (i=0;i<got;i+=written) {
            if (!WriteFile(tmp,copybuf+i,got-i,&written,NULL) || !written) goto done;
        }
    }
    if ((ULONGLONG)total!=(ULONGLONG)size.QuadPart || !FlushFileBuffers(tmp)) goto done;
    rc=7;zero.QuadPart=0;
    if (!SetFilePointerEx(src,zero,NULL,FILE_BEGIN) || !SetFilePointerEx(tmp,zero,NULL,FILE_BEGIN)) goto done;
    for (;;) {
        if (!ReadFile(src,copybuf,sizeof(copybuf),&got,NULL) ||
            !ReadFile(tmp,comparebuf,sizeof(comparebuf),&other,NULL) || got!=other) goto done;
        for (i=0;i<got;++i) if (copybuf[i]!=comparebuf[i]) goto done;
        if (!got) break;
    }
    if (!CloseHandle(tmp)) { tmp=INVALID_HANDLE_VALUE;goto done; }
    tmp=INVALID_HANDLE_VALUE;
    rc=9;
    if (!target_ok()) goto done;
    rc=8;
    if (clock_ms(1)>=PUBLISH_LIMIT) goto done;
    rc=12;
    if (!lease_ready(1)) goto done;
    rc=8;
    if (clock_ms(1)>=PUBLISH_LIMIT) goto done;
#if A04_ATOMIC_REPLACE_REFERENCE
    /* Deliberately leave closed, flushed temp as evidence, old dest untouched. */
    if (fault_before_publish) ExitProcess(90);
#endif
    rc=10;
    if (!MoveFileExW(temp,dest,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) goto done;
    rc=0;
done:
    if (tmp!=INVALID_HANDLE_VALUE) CloseHandle(tmp);
    if (src!=INVALID_HANDLE_VALUE) CloseHandle(src);
    while (parent_count) CloseHandle(parents[--parent_count]);
    ExitProcess(rc);
}
