#ifndef A04_TEST_WINIOCTL_H
#define A04_TEST_WINIOCTL_H
#include "windows.h"

typedef struct { DWORD DiskNumber; LARGE_INTEGER StartingOffset,ExtentLength; } DISK_EXTENT;
typedef struct { DWORD NumberOfDiskExtents; DISK_EXTENT Extents[1]; } VOLUME_DISK_EXTENTS;
typedef struct { DWORD PartitionStyle; LARGE_INTEGER StartingOffset,PartitionLength; DWORD PartitionNumber; struct {GUID PartitionType,PartitionId; WCHAR Name[36];} Gpt; } PARTITION_INFORMATION_EX;
typedef struct { DWORD PartitionStyle,PartitionCount; struct {GUID DiskId;} Gpt; PARTITION_INFORMATION_EX PartitionEntry[1]; } DRIVE_LAYOUT_INFORMATION_EX;
#define PARTITION_STYLE_GPT 1
#define IOCTL_DISK_GET_DRIVE_LAYOUT_EX 4
#define IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS 1
#define FSCTL_LOCK_VOLUME 2
#define FSCTL_DISMOUNT_VOLUME 3
#endif
