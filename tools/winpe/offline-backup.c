/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Fixed-path experimental backup/replay for WinPE. No update is performed.
 * The READY journal remains after replay. Never run while any other process
 * accesses the target volume. E: must be the existing recovery ESP. */
#include <stddef.h>
#include <windows.h>
#include <winioctl.h>
#include <wincrypt.h>

#define COUNT 11
#define CHUNK 65536
#define RESERVE (8ULL*1024*1024)
static const WCHAR *target[COUNT]={
 L"C:\\Windows\\System32\\config\\SYSTEM",L"C:\\Windows\\System32\\config\\SYSTEM.LOG",L"C:\\Windows\\System32\\config\\SYSTEM.LOG1",L"C:\\Windows\\System32\\config\\SYSTEM.LOG2",
 L"C:\\Windows\\System32\\config\\SOFTWARE",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG1",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG2",
 L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys",L"C:\\Windows\\System32\\drivers\\bu21025.sys"};
static const WCHAR *temp[COUNT]={
 L"C:\\Windows\\System32\\config\\SYSTEM.a04restore",L"C:\\Windows\\System32\\config\\SYSTEM.LOG.a04restore",L"C:\\Windows\\System32\\config\\SYSTEM.LOG1.a04restore",L"C:\\Windows\\System32\\config\\SYSTEM.LOG2.a04restore",
 L"C:\\Windows\\System32\\config\\SOFTWARE.a04restore",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG.a04restore",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG1.a04restore",L"C:\\Windows\\System32\\config\\SOFTWARE.LOG2.a04restore",
 L"C:\\Windows\\System32\\drivers\\mt8167-toprgu.sys.a04restore",L"C:\\Windows\\System32\\drivers\\mt8167-shared.sys.a04restore",L"C:\\Windows\\System32\\drivers\\bu21025.sys.a04restore"};
typedef struct { DWORD present, attributes; FILETIME creation,access,write; ULONGLONG length; BYTE digest[32]; } ENTRY;
typedef struct { DWORD magic, version, count, reserved; ENTRY entries[COUNT]; BYTE digest[32]; } MANIFEST;
static BYTE buf[CHUNK]; static MANIFEST manifest; static HCRYPTPROV provider;
static void say(const char *s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD x){const char *d="0123456789ABCDEF";char b[9];unsigned j;for(j=0;j<8;++j)b[j]=d[(x>>(28-4*j))&15];b[8]=0;say(b);}
static void fail(const char *s){say(s);say("\r\n");ExitProcess(1);}
static int same(const BYTE *a,const BYTE *b,DWORD n){DWORD i;BYTE v=0;for(i=0;i<n;++i)v|=a[i]^b[i];return v==0;}
static void path(unsigned i,WCHAR *p){const WCHAR *base=L"E:\\A04BACKUP\\item00.bin";unsigned j=0;while(base[j]){p[j]=base[j];++j;}p[j]=0;p[17]=(WCHAR)(L'0'+i/10);p[18]=(WCHAR)(L'0'+i%10);}
static int hash_start(HCRYPTHASH *h){return CryptCreateHash(provider,CALG_SHA_256,0,0,h)!=0;}
static int hash_end(HCRYPTHASH h,BYTE *out){DWORD n=32;BOOL ok=CryptGetHashParam(h,HP_HASHVAL,out,&n,0);CryptDestroyHash(h);return ok && n==32;}
static int hash_mem(const void *p,DWORD n,BYTE *out){HCRYPTHASH h;if(!hash_start(&h))return 0;if(!CryptHashData(h,(const BYTE*)p,n,0)){CryptDestroyHash(h);return 0;}return hash_end(h,out);}
static int write_all(HANDLE h,const BYTE *p,DWORD n){DWORD w;while(n){if(!WriteFile(h,p,n,&w,0)||!w)return 0;p+=w;n-=w;}return 1;}
static int volume(WCHAR drive,LONGLONG offset,LONGLONG length,const WCHAR *fs){
 WCHAR root[]=L"C:\\",dev[]=L"\\\\.\\C:",type[32]; VOLUME_DISK_EXTENTS x; DWORD n; HANDLE h;int ok;
 root[0]=drive;dev[4]=drive;if(!GetVolumeInformationW(root,0,0,0,0,0,type,32)||lstrcmpiW(type,fs))return 0;
 h=CreateFileW(dev,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;
 ok=DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,0,0,&x,sizeof(x),&n,0)&&n>=sizeof(x)&&x.NumberOfDiskExtents==1&&x.Extents[0].DiskNumber==0&&x.Extents[0].StartingOffset.QuadPart==offset&&x.Extents[0].ExtentLength.QuadPart==length;CloseHandle(h);return ok;
}
static int flush_volume(WCHAR drive){WCHAR dev[]=L"\\\\.\\C:";HANDLE h;int ok;dev[4]=drive;h=CreateFileW(dev,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;ok=FlushFileBuffers(h);if(!CloseHandle(h))ok=0;return ok;}
static int clean_windows_volume(void){HANDLE h;DWORD flags=0,n=0;int ok;
 h=CreateFileW(L"\\\\.\\C:",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;
 ok=DeviceIoControl(h,FSCTL_IS_VOLUME_DIRTY,0,0,&flags,sizeof(flags),&n,0)&&n==sizeof(flags)&&flags==0;
 if(!CloseHandle(h))ok=0;return ok;
}
static int privilege(const WCHAR *name){HANDLE t;TOKEN_PRIVILEGES p;LUID l;DWORD e;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&t))return 0;if(!LookupPrivilegeValueW(0,name,&l)){CloseHandle(t);return 0;}p.PrivilegeCount=1;p.Privileges[0].Luid=l;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;SetLastError(0);if(!AdjustTokenPrivileges(t,FALSE,&p,0,0,0)){CloseHandle(t);return 0;}e=GetLastError();CloseHandle(t);return e==ERROR_SUCCESS;}
static int absent_hive(const WCHAR *name){HKEY k;LONG e=RegOpenKeyExW(HKEY_LOCAL_MACHINE,name,0,KEY_READ,&k);if(e==ERROR_SUCCESS){RegCloseKey(k);return 0;}return e==ERROR_FILE_NOT_FOUND;}
static int prep(void){
 if(!volume(L'C',469762048LL,15147204608LL,L"NTFS")||!volume(L'E',184549376LL,268435456LL,L"FAT32"))return 0;
 if(!clean_windows_volume())return 0;
 if(GetFileAttributesW(L"C:\\Windows\\System32\\config\\SYSTEM")==INVALID_FILE_ATTRIBUTES||GetFileAttributesW(L"E:\\EFI\\Boot\\bootarm.efi")==INVALID_FILE_ATTRIBUTES)return 0;
 if(!absent_hive(L"A04TOPRGUSYS")||!absent_hive(L"A04TOPRGUSW")||!absent_hive(L"A04TEMP")||!absent_hive(L"A04TXSYS")||!absent_hive(L"A04TXSW"))return 0;
 if(!privilege(L"SeBackupPrivilege")||!privilege(L"SeRestorePrivilege")||!privilege(L"SeSecurityPrivilege"))return 0;
 return CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
}
/* Exclusive access prevents taking a snapshot of a file another process holds.
 * BackupRead/BackupWrite preserve streams and security in the archive. */
static int measure_item(unsigned i,ENTRY *e){
 HANDLE src;BY_HANDLE_FILE_INFORMATION info;HCRYPTHASH h;void *ctx=0;DWORD n;int ok=0;
 src=CreateFileW(target[i],GENERIC_READ|READ_CONTROL|ACCESS_SYSTEM_SECURITY,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,0);
 if(src==INVALID_HANDLE_VALUE){DWORD x=GetLastError();return x==ERROR_FILE_NOT_FOUND && i!=0 && i!=4;}
 if(!GetFileInformationByHandle(src,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ENCRYPTED|FILE_ATTRIBUTE_COMPRESSED|FILE_ATTRIBUTE_SPARSE_FILE|FILE_ATTRIBUTE_OFFLINE)))goto done;
 e->present=1;e->attributes=info.dwFileAttributes;e->creation=info.ftCreationTime;e->access=info.ftLastAccessTime;e->write=info.ftLastWriteTime;
 if(!hash_start(&h))goto done;
 for(;;){if(!BackupRead(src,buf,CHUNK,&n,FALSE,TRUE,&ctx))goto hashfail;if(!n)break;if(e->length>~(ULONGLONG)0-n||!CryptHashData(h,buf,n,0))goto hashfail;e->length+=n;}
 if(!BackupRead(src,0,0,&n,TRUE,TRUE,&ctx))goto hashfail;ctx=0;
 if(!hash_end(h,e->digest))goto done;ok=1;goto done;
 hashfail:CryptDestroyHash(h);if(ctx)BackupRead(src,0,0,&n,TRUE,TRUE,&ctx);
 done:if(!CloseHandle(src))ok=0;return ok;
}
static int same_entry(const ENTRY *a,const ENTRY *b){
 if(a->present!=b->present)return 0;if(!a->present)return 1;
 return a->attributes==b->attributes && !CompareFileTime(&a->creation,&b->creation) &&
        !CompareFileTime(&a->write,&b->write) && a->length==b->length && same(a->digest,b->digest,32);
}
static int save_item(unsigned i){
 HANDLE src,dst;BY_HANDLE_FILE_INFORMATION info;HCRYPTHASH h;void *ctx=0;DWORD n;WCHAR p[64];ENTRY *e=&manifest.entries[i];int ok=0;
 src=CreateFileW(target[i],GENERIC_READ|READ_CONTROL|ACCESS_SYSTEM_SECURITY,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,0);
 if(src==INVALID_HANDLE_VALUE){DWORD x=GetLastError();return (x==ERROR_FILE_NOT_FOUND && i!=0 && i!=4);}
 if(!GetFileInformationByHandle(src,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_ENCRYPTED|FILE_ATTRIBUTE_COMPRESSED|FILE_ATTRIBUTE_SPARSE_FILE|FILE_ATTRIBUTE_OFFLINE)))goto endsrc;
 e->present=1;e->attributes=info.dwFileAttributes;e->creation=info.ftCreationTime;e->access=info.ftLastAccessTime;e->write=info.ftLastWriteTime;
 path(i,p);dst=CreateFileW(p,GENERIC_WRITE,0,0,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,0);if(dst==INVALID_HANDLE_VALUE)goto endsrc;
 if(!hash_start(&h))goto enddst;
 for(;;){if(!BackupRead(src,buf,CHUNK,&n,FALSE,TRUE,&ctx))goto endhash;if(!n)break;if(!CryptHashData(h,buf,n,0)||!write_all(dst,buf,n))goto endhash;e->length+=n;}
 if(!BackupRead(src,0,0,&n,TRUE,TRUE,&ctx))goto endhash;
 if(!hash_end(h,e->digest))goto enddst;h=0;
 if(!FlushFileBuffers(dst))goto enddst;ok=1;goto enddst;
 endhash:CryptDestroyHash(h);
 enddst:if(ctx)BackupRead(src,0,0,&n,TRUE,TRUE,&ctx);if(!CloseHandle(dst))ok=0;
 endsrc:if(!CloseHandle(src))ok=0;return ok;
}
static int verify_item(unsigned i){WCHAR p[64];HANDLE h;HCRYPTHASH hash;BYTE digest[32];DWORD n;ULONGLONG total=0;int ok=0;ENTRY *e=&manifest.entries[i];if(!e->present)return 1;path(i,p);h=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;if(!hash_start(&hash)){CloseHandle(h);return 0;}for(;;){if(!ReadFile(h,buf,CHUNK,&n,0))goto done;if(!n)break;total+=n;if(!CryptHashData(hash,buf,n,0))goto done;}if(!hash_end(hash,digest)){hash=0;goto close;}hash=0;ok=total==e->length&&same(digest,e->digest,32);goto close;done:CryptDestroyHash(hash);close:if(!CloseHandle(h))ok=0;return ok;}
static int verify_manifest(void){BYTE d[32];unsigned i;if(manifest.magic!=0x41303442||manifest.version!=1||manifest.count!=COUNT||manifest.reserved)return 0;if(!hash_mem(&manifest,(DWORD)offsetof(MANIFEST,digest),d)||!same(d,manifest.digest,32))return 0;for(i=0;i<COUNT;++i)if(manifest.entries[i].present>1||!verify_item(i))return 0;return manifest.entries[0].present&&manifest.entries[4].present;}
static int load_manifest(void){HANDLE h;DWORD n;BYTE extra;h=CreateFileW(L"E:\\A04BACKUP\\READY",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;if(!ReadFile(h,&manifest,sizeof(manifest),&n,0)||n!=sizeof(manifest)){CloseHandle(h);return 0;}if(!ReadFile(h,&extra,1,&n,0)||n){CloseHandle(h);return 0;}CloseHandle(h);return verify_manifest();}
static int verify_restored(unsigned i){
 HANDLE f;BY_HANDLE_FILE_INFORMATION info;HCRYPTHASH h;BYTE digest[32];void *ctx=0;DWORD n;ULONGLONG total=0;int ok=0;ENTRY *e=&manifest.entries[i];
 if(!e->present){if(GetFileAttributesW(target[i])!=INVALID_FILE_ATTRIBUTES)return 0;return GetLastError()==ERROR_FILE_NOT_FOUND;}
 f=CreateFileW(target[i],GENERIC_READ|READ_CONTROL|ACCESS_SYSTEM_SECURITY,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,0);if(f==INVALID_HANDLE_VALUE)return 0;
 if(!GetFileInformationByHandle(f,&info)){say("RECOVERY_INFO_FAILED\r\n");goto close;}
 if(info.nNumberOfLinks!=1){say("RECOVERY_LINK_MISMATCH\r\n");goto close;}
 if(info.dwFileAttributes!=e->attributes){say("RECOVERY_ATTRIBUTES_MISMATCH actual=");hex(info.dwFileAttributes);say(" expected=");hex(e->attributes);say("\r\n");goto close;}
 if(CompareFileTime(&info.ftCreationTime,&e->creation)){say("RECOVERY_CREATION_MISMATCH\r\n");goto close;}
 if(CompareFileTime(&info.ftLastWriteTime,&e->write)){say("RECOVERY_WRITE_TIME_MISMATCH\r\n");goto close;}
 if(!hash_start(&h))goto close;
 for(;;){if(!BackupRead(f,buf,CHUNK,&n,FALSE,TRUE,&ctx))goto hashfail;if(!n)break;total+=n;if(!CryptHashData(h,buf,n,0))goto hashfail;}
 if(!BackupRead(f,0,0,&n,TRUE,TRUE,&ctx))goto hashfail;ctx=0;
 if(!hash_end(h,digest))goto close;ok=(total==e->length&&same(digest,e->digest,32));if(!ok)say("RECOVERY_STREAM_MISMATCH\r\n");goto close;
 hashfail:CryptDestroyHash(h);if(ctx)BackupRead(f,0,0,&n,TRUE,TRUE,&ctx);
 close:if(!CloseHandle(f))ok=0;return ok;
}
#ifdef A04_OFFLINE_FIXTURE
static int fixture_index(const char *name,unsigned i){char s[8];DWORD n=GetEnvironmentVariableA(name,s,sizeof(s));unsigned value;if(n==1 && s[0]>='0' && s[0]<='9')value=(unsigned)(s[0]-'0');else if(n==2 && s[0]=='1' && s[1]=='0')value=10;else return 0;return value==i;}
static void fixture_temp_fault(unsigned i){if(fixture_index("A04_FAIL_TEMP",i))ExitProcess(98);}
#else
static void fixture_temp_fault(unsigned i){(void)i;}
#endif
static int replay_item(unsigned i){WCHAR p[64];HANDLE in,out;void *ctx=0;DWORD n,w,attr;int ok=0;ENTRY *e=&manifest.entries[i];
 if(!e->present){attr=GetFileAttributesW(target[i]);if(attr==INVALID_FILE_ATTRIBUTES)return GetLastError()==ERROR_FILE_NOT_FOUND;if(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))return 0;if(!SetFileAttributesW(target[i],FILE_ATTRIBUTE_NORMAL))return 0;return DeleteFileW(target[i])!=0;}
 attr=GetFileAttributesW(temp[i]);if(attr!=INVALID_FILE_ATTRIBUTES){if(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))return 0;if(!SetFileAttributesW(temp[i],FILE_ATTRIBUTE_NORMAL)||!DeleteFileW(temp[i]))return 0;}else if(GetLastError()!=ERROR_FILE_NOT_FOUND)return 0;
 path(i,p);in=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(in==INVALID_HANDLE_VALUE)return 0;
 out=CreateFileW(temp[i],GENERIC_WRITE|WRITE_DAC|WRITE_OWNER|ACCESS_SYSTEM_SECURITY,0,0,CREATE_NEW,FILE_FLAG_WRITE_THROUGH|FILE_FLAG_BACKUP_SEMANTICS,0);if(out==INVALID_HANDLE_VALUE){CloseHandle(in);return 0;}
 for(;;){if(!ReadFile(in,buf,CHUNK,&n,0))goto done;if(!n)break;if(!BackupWrite(out,buf,n,&w,FALSE,TRUE,&ctx)||w!=n)goto done;}
 if(!BackupWrite(out,0,0,&w,TRUE,TRUE,&ctx))goto done;ctx=0;
 if(!SetFileTime(out,&e->creation,&e->access,&e->write)||!FlushFileBuffers(out))goto done;
 ok=1;
 done:if(ctx)BackupWrite(out,0,0,&w,TRUE,TRUE,&ctx);if(!CloseHandle(out))ok=0;if(!CloseHandle(in))ok=0;if(!ok)return 0;
 if(!SetFileAttributesW(temp[i],e->attributes))return 0;
 fixture_temp_fault(i);
 attr=GetFileAttributesW(target[i]);
 if(attr!=INVALID_FILE_ATTRIBUTES){if(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))return 0;if(!SetFileAttributesW(target[i],FILE_ATTRIBUTE_NORMAL))return 0;}
 else if(GetLastError()!=ERROR_FILE_NOT_FOUND)return 0;
 if(!MoveFileExW(temp[i],target[i],MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return 0;
 return SetFileAttributesW(target[i],e->attributes)!=0;
}
#ifdef A04_OFFLINE_FIXTURE
static int fixture_full(void){char value[4];return GetEnvironmentVariableA("A04_FAKE_FULL",value,sizeof(value))==1 && value[0]=='1';}
#else
static int fixture_full(void){return 0;}
#endif
/* The fixed journal does not yet capture transaction-manager sidecars.
 * Refuse snapshotting when either hive has them; absence must be known. */
static int no_hive_sidecars(const WCHAR *pattern){WIN32_FIND_DATAW data;HANDLE h=FindFirstFileW(pattern,&data);DWORD e;
 if(h!=INVALID_HANDLE_VALUE){FindClose(h);return 0;}
 e=GetLastError();return e==ERROR_FILE_NOT_FOUND;
}
static void backup(void){unsigned i;ULARGE_INTEGER free;ULONGLONG need=RESERVE,cluster,rounded;HANDLE ready;DWORD spc,bps,avail,total;static ENTRY measured[COUNT];
 if(GetFileAttributesW(L"E:\\A04BACKUP")!=INVALID_FILE_ATTRIBUTES)fail("BACKUP_DIRECTORY_EXISTS");
 if(!no_hive_sidecars(L"C:\\Windows\\System32\\config\\SYSTEM{*")||!no_hive_sidecars(L"C:\\Windows\\System32\\config\\SOFTWARE{*"))fail("BACKUP_UNSUPPORTED_HIVE_SIDECARS");
 if(!GetDiskFreeSpaceW(L"E:\\",&spc,&bps,&avail,&total)||!spc||!bps)fail("BACKUP_CAPACITY_UNKNOWN");cluster=(ULONGLONG)spc*bps;if(!cluster || (cluster & (cluster-1)))fail("BACKUP_CLUSTER_REJECTED");
 for(i=0;i<COUNT;++i){
  if(GetFileAttributesW(temp[i])!=INVALID_FILE_ATTRIBUTES)fail("BACKUP_TEMP_EXISTS");
  if(!measure_item(i,&measured[i]))fail("BACKUP_MEASURE_FAILED_NO_UPDATE");
  if(measured[i].length>~(ULONGLONG)0-(cluster-1))fail("BACKUP_CAPACITY_OVERFLOW");
  rounded=(measured[i].length+cluster-1)&~(cluster-1);
  if(need>~(ULONGLONG)0-rounded)fail("BACKUP_CAPACITY_OVERFLOW");need+=rounded;
 }
 if(fixture_full()||!GetDiskFreeSpaceExW(L"E:\\",&free,0,0)||free.QuadPart<need)fail("BACKUP_CAPACITY_REJECTED");
 if(!CreateDirectoryW(L"E:\\A04BACKUP",0))fail("BACKUP_DIRECTORY_FAILED");
 manifest.magic=0x41303442;manifest.version=1;manifest.count=COUNT;
 for(i=0;i<COUNT;++i)if(!save_item(i)||!same_entry(&manifest.entries[i],&measured[i]))fail("BACKUP_ITEM_CHANGED_OR_FAILED_NO_UPDATE");
 if(!hash_mem(&manifest,(DWORD)offsetof(MANIFEST,digest),manifest.digest)||!verify_manifest()||!flush_volume(L'E'))fail("BACKUP_VERIFY_FAILED_NO_UPDATE");
 ready=CreateFileW(L"E:\\A04BACKUP\\READY",GENERIC_WRITE,0,0,CREATE_NEW,FILE_FLAG_WRITE_THROUGH,0);if(ready==INVALID_HANDLE_VALUE)fail("BACKUP_READY_CREATE_FAILED");
 if(!write_all(ready,(BYTE*)&manifest,sizeof(manifest))||!FlushFileBuffers(ready)){CloseHandle(ready);fail("BACKUP_READY_WRITE_FAILED");}if(!CloseHandle(ready)||!flush_volume(L'E'))fail("BACKUP_READY_FLUSH_FAILED");
 if(!load_manifest())fail("BACKUP_READY_VERIFY_FAILED");say("BACKUP_READY_NO_UPDATE_PERFORMED\r\n");
}
static int target_exclusive(unsigned i){HANDLE h;BY_HANDLE_FILE_INFORMATION info;DWORD e;
 h=CreateFileW(target[i],GENERIC_READ|READ_CONTROL,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,0);
 if(h==INVALID_HANDLE_VALUE){e=GetLastError();return e==ERROR_FILE_NOT_FOUND;}
 if(!GetFileInformationByHandle(h,&info)||info.nNumberOfLinks!=1||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))){CloseHandle(h);return 0;}
 return CloseHandle(h)!=0;
}
#ifdef A04_OFFLINE_FIXTURE
static void fixture_fault(unsigned i){if(fixture_index("A04_FAIL_AFTER",i))ExitProcess(98);}
#else
static void fixture_fault(unsigned i){(void)i;}
#endif
static void restore(void){unsigned i;if(!load_manifest())fail("RECOVERY_JOURNAL_REJECTED");if(!no_hive_sidecars(L"C:\\Windows\\System32\\config\\SYSTEM{*")||!no_hive_sidecars(L"C:\\Windows\\System32\\config\\SOFTWARE{*"))fail("RECOVERY_UNSUPPORTED_HIVE_SIDECARS");for(i=0;i<COUNT;++i)if(!target_exclusive(i))fail("RECOVERY_TARGET_BUSY_OR_UNSAFE");for(i=0;i<COUNT;++i){if(!replay_item(i))fail("RECOVERY_REPLAY_FAILED_RETRY_FROM_WINPE");if(!flush_volume(L'C'))fail("RECOVERY_FLUSH_FAILED_RETRY_FROM_WINPE");if(!verify_restored(i))fail("RECOVERY_VERIFY_FAILED_RETRY_FROM_WINPE");fixture_fault(i);}say("RECOVERY_FILES_REPLAYED_DISMOUNT_REQUIRED\r\n");}
void mainCRTStartup(void){const WCHAR *c=GetCommandLineW();int restore_mode=0;while(*c){if(c[0]==L'-'&&c[1]==L'-'&&c[2]==L'r'&&c[3]==L'e'&&c[4]==L's'&&c[5]==L't'&&c[6]==L'o'&&c[7]==L'r'&&c[8]==L'e'&&(c[9]==0||c[9]==L' ')){restore_mode=1;break;}++c;}if(!prep())fail("BACKUP_IDENTITY_OR_PRIVILEGE_REJECTED");if(restore_mode)restore();else backup();CryptReleaseContext(provider,0);ExitProcess(0);}
