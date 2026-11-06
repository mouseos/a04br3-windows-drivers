#ifndef A04_TEST_WINIOCTL_H
#define A04_TEST_WINIOCTL_H
#include "windows.h"
typedef struct { long long QuadPart; } LARGE_INTEGER;
typedef struct { DWORD DiskNumber; LARGE_INTEGER StartingOffset,ExtentLength; } DISK_EXTENT;
typedef struct { DWORD NumberOfDiskExtents; DISK_EXTENT Extents[1]; } VOLUME_DISK_EXTENTS;
#define IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS 1
#define FSCTL_LOCK_VOLUME 2
#define FSCTL_DISMOUNT_VOLUME 3
#endif
