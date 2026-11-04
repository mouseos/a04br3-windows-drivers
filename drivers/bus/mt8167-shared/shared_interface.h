#ifndef MT8167_SHARED_INTERFACE_H
#define MT8167_SHARED_INTERFACE_H
#include <ntddk.h>
/* Private versioned kernel interface; no ACPI or client is currently wired. */
DEFINE_GUID(GUID_A04_MT8167_SHARED_INTERFACE,
    0x78a0f3ac,0xd4ac,0x4c45,0x9b,0x6d,0x32,0xaf,0x0b,0x08,0x31,0x17);
typedef struct _A04_LEGACY_SHARED_INTERFACE {
    INTERFACE Header;
    NTSTATUS (*ReadButtons)(PVOID context, USHORT *value);
} A04_LEGACY_SHARED_INTERFACE;
#endif
