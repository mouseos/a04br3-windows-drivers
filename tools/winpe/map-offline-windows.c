/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Assign C: only to a uniquely identified offline Windows volume. */
#include <windows.h>
#include <winioctl.h>
#include <stddef.h>
static void say(const char*s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void hex(DWORD v){const char*h="0123456789ABCDEF";char b[9];unsigned i;for(i=0;i<8;i++)b[i]=h[(v>>(28-4*i))&15];b[8]=0;say(b);}
static void wide(const WCHAR*s){char c[2];c[1]=0;while(*s){c[0]=(*s>=32&&*s<127)?(char)*s:'?';say(c);s++;}}
static int extent(const WCHAR*name,int*match){WCHAR device[128];DWORD n=0;HANDLE h;VOLUME_DISK_EXTENTS ex;unsigned len=lstrlenW(name);*match=0;
 if(len<5||len>=128||name[len-1]!=L'\\')return 0;lstrcpyW(device,name);device[len-1]=0;
 h=CreateFileW(device,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE){say("A04MAP_EXTENT_OPEN_ERROR=");hex(GetLastError());say("\r\n");return 0;}
 if(!DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,0,0,&ex,sizeof(ex),&n,0)||n<sizeof(ex)){say("A04MAP_EXTENT_IOCTL_ERROR=");hex(GetLastError());say("\r\n");CloseHandle(h);return 0;}
 say("A04MAP_EXTENT_COUNT=");hex(ex.NumberOfDiskExtents);if(ex.NumberOfDiskExtents==1){say(" DISK=");hex(ex.Extents[0].DiskNumber);say(" OFFSET=");hex((DWORD)((ULONGLONG)ex.Extents[0].StartingOffset.QuadPart>>32));hex((DWORD)ex.Extents[0].StartingOffset.QuadPart);say(" LENGTH=");hex((DWORD)((ULONGLONG)ex.Extents[0].ExtentLength.QuadPart>>32));hex((DWORD)ex.Extents[0].ExtentLength.QuadPart);}say("\r\n");
 if(ex.NumberOfDiskExtents==1&&ex.Extents[0].DiskNumber==0&&ex.Extents[0].StartingOffset.QuadPart==469762048LL&&ex.Extents[0].ExtentLength.QuadPart==15147204608LL)*match=1;
 return CloseHandle(h)!=0;}
/* With automount disabled, the partition volume can exist before NTFS has
 * been mounted. Before a provisional letter, verify disk0 GPT identity and
 * the on-disk NTFS boot sector with read-only disk operations. */
static BYTE disk_layout[65536],sector[512];
static int raw_target(void){HANDLE h;DWORD n=0,i,found=0;DRIVE_LAYOUT_INFORMATION_EX*d=(DRIVE_LAYOUT_INFORMATION_EX*)disk_layout;LARGE_INTEGER at;unsigned long long sectors=0;PARTITION_INFORMATION_EX*p;
 h=CreateFileW(L"\\\\.\\PhysicalDrive0",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE){say("A04MAP_RAW_DISK_OPEN_ERROR=");hex(GetLastError());say("\r\n");return 0;}
 if(!DeviceIoControl(h,IOCTL_DISK_GET_DRIVE_LAYOUT_EX,0,0,disk_layout,sizeof(disk_layout),&n,0)||n<offsetof(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry)){say("A04MAP_GPT_QUERY_ERROR=");hex(GetLastError());say("\r\n");CloseHandle(h);return 0;}
 if(d->PartitionStyle!=PARTITION_STYLE_GPT||d->Gpt.DiskId.Data1!=0xe347eb68||d->Gpt.DiskId.Data2!=0x4ff8||d->Gpt.DiskId.Data3!=0x5143||d->Gpt.DiskId.Data4[0]!=0xa3||d->Gpt.DiskId.Data4[1]!=0x34||d->Gpt.DiskId.Data4[2]!=0x08||d->Gpt.DiskId.Data4[3]!=0x86||d->Gpt.DiskId.Data4[4]!=0x80||d->Gpt.DiskId.Data4[5]!=0x3f||d->Gpt.DiskId.Data4[6]!=0xd8||d->Gpt.DiskId.Data4[7]!=0xfa||d->PartitionCount>(n-(DWORD)offsetof(DRIVE_LAYOUT_INFORMATION_EX,PartitionEntry))/sizeof(PARTITION_INFORMATION_EX)||d->PartitionCount>128){say("A04MAP_GPT_LAYOUT_REJECTED\r\n");CloseHandle(h);return 0;}
 for(i=0;i<d->PartitionCount;i++){GUID*g;p=&d->PartitionEntry[i];if(p->PartitionStyle!=PARTITION_STYLE_GPT||p->StartingOffset.QuadPart!=469762048LL||p->PartitionLength.QuadPart!=15147204608LL)continue;g=&p->Gpt.PartitionType;
 if(p->Gpt.PartitionId.Data1!=0xf31443dd||p->Gpt.PartitionId.Data2!=0x566a||p->Gpt.PartitionId.Data3!=0x5dbe||p->Gpt.PartitionId.Data4[0]!=0x86||p->Gpt.PartitionId.Data4[1]!=0xc6||p->Gpt.PartitionId.Data4[2]!=0xeb||p->Gpt.PartitionId.Data4[3]!=0xc1||p->Gpt.PartitionId.Data4[4]!=0xb3||p->Gpt.PartitionId.Data4[5]!=0x0b||p->Gpt.PartitionId.Data4[6]!=0x65||p->Gpt.PartitionId.Data4[7]!=0xa1||p->PartitionNumber!=20||(CompareStringOrdinal(p->Gpt.Name,7,L"Windows",7,TRUE)!=CSTR_EQUAL||p->Gpt.Name[7]!=0)||g->Data1!=0xebd0a0a2||g->Data2!=0xb9e5||g->Data3!=0x4433||g->Data4[0]!=0x87||g->Data4[1]!=0xc0||g->Data4[2]!=0x68||g->Data4[3]!=0xb6||g->Data4[4]!=0xb7||g->Data4[5]!=0x26||g->Data4[6]!=0x99||g->Data4[7]!=0xc7){say("A04MAP_GPT_IDENTITY_REJECTED\r\n");CloseHandle(h);return 0;}found++;}
 if(found!=1){say("A04MAP_GPT_MATCH_COUNT_REJECTED\r\n");CloseHandle(h);return 0;}
 at.QuadPart=469762048LL;if(!SetFilePointerEx(h,at,0,FILE_BEGIN)||!ReadFile(h,sector,sizeof(sector),&n,0)||n!=sizeof(sector)){say("A04MAP_BOOT_READ_ERROR=");hex(GetLastError());say("\r\n");CloseHandle(h);return 0;}
 for(i=0;i<8;i++)if(sector[3+i]!=(BYTE)"NTFS    "[i]){say("A04MAP_BOOT_OEM_REJECTED\r\n");CloseHandle(h);return 0;}
 for(i=0;i<8;i++)sectors|=((unsigned long long)sector[40+i])<<(8*i);
 if(sector[11]!=0||sector[12]!=2||sector[510]!=0x55||sector[511]!=0xaa||sector[28]!=0||sector[29]!=0||sector[30]!=0x0e||sector[31]!=0||!sectors||sectors>15147204608ULL/512){say("A04MAP_BOOT_GEOMETRY_REJECTED\r\n");CloseHandle(h);return 0;}
 if(!CloseHandle(h)){say("A04MAP_RAW_CLOSE_FAILED\r\n");return 0;}say("A04MAP_GPT_AND_NTFS_BOOT_VERIFIED_READ_ONLY\r\n");return 1;}
static int filesystem(const WCHAR*root){WCHAR label[64],fs[32],marker[256];DWORD attr;
 if(!GetVolumeInformationW(root,label,64,0,0,0,fs,32)){DWORD error=GetLastError();say("A04MAP_MATCH_INFO_ERROR=");hex(error);say("\r\n");return error==ERROR_NOT_READY?-1:0;}
 say("A04MAP_MATCH_LABEL=");wide(label);say(" FS=");wide(fs);say("\r\n");if(lstrcmpiW(label,L"Windows")){say("A04MAP_MATCH_LABEL_REJECTED\r\n");return 0;}if(lstrcmpiW(fs,L"NTFS")){say("A04MAP_MATCH_FS_REJECTED\r\n");return 0;}
 attr=GetFileAttributesW(root);if(attr==INVALID_FILE_ATTRIBUTES||(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))!=(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)){say("A04MAP_MATCH_ROOT_REJECTED\r\n");return 0;}
 lstrcpyW(marker,root);lstrcatW(marker,L"Windows\\System32\\config\\SYSTEM");attr=GetFileAttributesW(marker);if(attr==INVALID_FILE_ATTRIBUTES||(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){say("A04MAP_MATCH_MARKER_REJECTED\r\n");return 0;}return 1;}
static void remove_own_mapping(const WCHAR*chosen){WCHAR mapped[128];if(!GetVolumeNameForVolumeMountPointW(L"C:\\",mapped,128)||lstrcmpiW(mapped,chosen)){say("A04MAP_CLEANUP_IDENTITY_UNKNOWN_NO_REMOVE\r\n");return;}if(!DeleteVolumeMountPointW(L"C:\\")){say("A04MAP_CLEANUP_REMOVE_ERROR=");hex(GetLastError());say("\r\n");return;}say("A04MAP_CLEANUP_LETTER_REMOVED_DISMOUNT_UNVERIFIED\r\n");}
void mainCRTStartup(void){WCHAR name[128],ram[128],chosen[128],label[64],fs[32],windows[128];HKEY key;HANDLE it;DWORD drives,err,attempt;int match,found=0,needs_mount=0;
 if(!GetWindowsDirectoryW(windows,128)||lstrcmpiW(windows,L"X:\\Windows")||RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_READ,&key)!=ERROR_SUCCESS){say("A04MAP_NOT_WINPE\r\n");ExitProcess(2);}RegCloseKey(key);
 drives=GetLogicalDrives();if(!drives){say("A04MAP_DRIVES_UNKNOWN\r\n");ExitProcess(2);}if(drives&(1u<<2)){say("A04MAP_C_ALREADY_PRESENT_VALIDATE_WITH_GUARD\r\n");ExitProcess(0);}
 if(!GetVolumeNameForVolumeMountPointW(L"X:\\",ram,128)){say("A04MAP_WINPE_VOLUME_UNKNOWN\r\n");ExitProcess(2);}
 it=FindFirstVolumeW(name,128);if(it==INVALID_HANDLE_VALUE){say("A04MAP_ENUMERATION_FAILED\r\n");ExitProcess(2);}
 for(;;){say("A04MAP_VOLUME=");wide(name);say("\r\n");if(lstrcmpiW(name,ram)){if(!extent(name,&match)){say("A04MAP_VOLUME_IDENTITY_UNKNOWN\r\n");FindVolumeClose(it);ExitProcess(2);}if(match){
 if(!GetVolumeInformationW(name,label,64,0,0,0,fs,32)){err=GetLastError();say("A04MAP_MATCH_INFO_ERROR=");hex(err);say("\r\n");if(err!=ERROR_NOT_READY){FindVolumeClose(it);ExitProcess(2);}needs_mount=1;say("A04MAP_PROVISIONAL_MOUNT_REQUIRED\r\n");}
 else if(filesystem(name)!=1){FindVolumeClose(it);ExitProcess(2);}
 if(++found!=1){say("A04MAP_MULTIPLE_MATCHES\r\n");FindVolumeClose(it);ExitProcess(2);}lstrcpyW(chosen,name);}}
 if(FindNextVolumeW(it,name,128))continue;err=GetLastError();if(err!=ERROR_NO_MORE_FILES){say("A04MAP_ENUMERATION_INCOMPLETE\r\n");FindVolumeClose(it);ExitProcess(2);}break;}
 if(!FindVolumeClose(it)){say("A04MAP_ENUMERATION_CLOSE_FAILED\r\n");ExitProcess(2);}if(found!=1){say("A04MAP_TARGET_NOT_FOUND\r\n");ExitProcess(4);}
 drives=GetLogicalDrives();if(!drives||(drives&(1u<<2))){say("A04MAP_C_CHANGED\r\n");ExitProcess(2);}
 if(needs_mount&&!raw_target()){say("A04MAP_PROVISIONAL_IDENTITY_REJECTED\r\n");ExitProcess(2);}
 if(!SetVolumeMountPointW(L"C:\\",chosen)){say("A04MAP_ASSIGN_FAILED\r\n");ExitProcess(3);}
 say("A04MAP_ASSIGNED_VALIDATE_FILESYSTEM\r\n");
 for(attempt=0;attempt<3;attempt++){int ready=filesystem(L"C:\\");if(ready==1){say("A04MAP_ASSIGNED_REQUIRES_GUARD\r\n");ExitProcess(0);}if(ready!=-1)break;if(attempt<2){say("A04MAP_READINESS_WAIT_2S\r\n");Sleep(2000);}}
 say("A04MAP_POST_MOUNT_VALIDATION_FAILED\r\n");remove_own_mapping(chosen);ExitProcess(2);}
