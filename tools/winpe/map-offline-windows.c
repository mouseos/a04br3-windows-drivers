/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Assign C: only to a uniquely identified offline Windows volume. */
#include <windows.h>
#include <winioctl.h>
static void say(const char*s){DWORD n=0,w;while(s[n])++n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static int extent(const WCHAR*name,int*match){WCHAR device[128];DWORD n=0;HANDLE h;VOLUME_DISK_EXTENTS ex;unsigned len=lstrlenW(name);*match=0;
 if(len<5||len>=128||name[len-1]!=L'\\')return 0;lstrcpyW(device,name);device[len-1]=0;
 h=CreateFileW(device,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)return 0;
 if(!DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,0,0,&ex,sizeof(ex),&n,0)||n<sizeof(ex)){CloseHandle(h);return 0;}
 if(ex.NumberOfDiskExtents==1&&ex.Extents[0].DiskNumber==0&&ex.Extents[0].StartingOffset.QuadPart==469762048LL&&ex.Extents[0].ExtentLength.QuadPart==15147204608LL)*match=1;
 return CloseHandle(h)!=0;}
void mainCRTStartup(void){WCHAR name[128],ram[128],chosen[128],label[64],fs[32],marker[256],windows[128];HKEY key;HANDLE it;DWORD drives,err,attr;int match,found=0;
 if(!GetWindowsDirectoryW(windows,128)||lstrcmpiW(windows,L"X:\\Windows")||RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\MiniNT",0,KEY_READ,&key)!=ERROR_SUCCESS){say("A04MAP_NOT_WINPE\r\n");ExitProcess(2);}RegCloseKey(key);
 drives=GetLogicalDrives();if(!drives){say("A04MAP_DRIVES_UNKNOWN\r\n");ExitProcess(2);}if(drives&(1u<<2)){say("A04MAP_C_ALREADY_PRESENT_VALIDATE_WITH_GUARD\r\n");ExitProcess(0);}
 if(!GetVolumeNameForVolumeMountPointW(L"X:\\",ram,128)){say("A04MAP_WINPE_VOLUME_UNKNOWN\r\n");ExitProcess(2);}
 it=FindFirstVolumeW(name,128);if(it==INVALID_HANDLE_VALUE){say("A04MAP_ENUMERATION_FAILED\r\n");ExitProcess(2);}
 for(;;){if(lstrcmpiW(name,ram)){if(!extent(name,&match)){say("A04MAP_VOLUME_IDENTITY_UNKNOWN\r\n");FindVolumeClose(it);ExitProcess(2);}if(match){
 if(!GetVolumeInformationW(name,label,64,0,0,0,fs,32)||lstrcmpiW(label,L"Windows")||lstrcmpiW(fs,L"NTFS")){say("A04MAP_MATCH_LABEL_OR_FS_REJECTED\r\n");FindVolumeClose(it);ExitProcess(2);}
 attr=GetFileAttributesW(name);if(attr==INVALID_FILE_ATTRIBUTES||(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))!=(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)){say("A04MAP_MATCH_ROOT_REJECTED\r\n");FindVolumeClose(it);ExitProcess(2);}
 lstrcpyW(marker,name);lstrcatW(marker,L"Windows\\System32\\config\\SYSTEM");attr=GetFileAttributesW(marker);if(attr==INVALID_FILE_ATTRIBUTES||(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){say("A04MAP_MATCH_MARKER_REJECTED\r\n");FindVolumeClose(it);ExitProcess(2);}
 if(++found!=1){say("A04MAP_MULTIPLE_MATCHES\r\n");FindVolumeClose(it);ExitProcess(2);}lstrcpyW(chosen,name);}}
 if(FindNextVolumeW(it,name,128))continue;err=GetLastError();if(err!=ERROR_NO_MORE_FILES){say("A04MAP_ENUMERATION_INCOMPLETE\r\n");FindVolumeClose(it);ExitProcess(2);}break;}
 if(!FindVolumeClose(it)){say("A04MAP_ENUMERATION_CLOSE_FAILED\r\n");ExitProcess(2);}if(found!=1){say("A04MAP_TARGET_NOT_FOUND\r\n");ExitProcess(2);}
 drives=GetLogicalDrives();if(!drives||(drives&(1u<<2))){say("A04MAP_C_CHANGED\r\n");ExitProcess(2);}
 if(!SetVolumeMountPointW(L"C:\\",chosen)){say("A04MAP_ASSIGN_FAILED\r\n");ExitProcess(3);}
 say("A04MAP_ASSIGNED_REQUIRES_GUARD\r\n");ExitProcess(0);}
