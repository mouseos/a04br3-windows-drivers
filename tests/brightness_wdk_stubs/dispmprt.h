/* SPDX-License-Identifier: BSD-2-Clause-Patent -- exact v1 callback shape from public dispmprt.h. */
#ifndef A04_BRIGHTNESS_TEST_DISPMPRT_H
#define A04_BRIGHTNESS_TEST_DISPMPRT_H
#include "ntddk.h"
#define DXGK_BRIGHTNESS_INTERFACE_VERSION_1 1
typedef void (*PINTERFACE_REFERENCE)(PVOID);
typedef void (*PINTERFACE_DEREFERENCE)(PVOID);
typedef struct {USHORT Size,Version;PVOID Context;PINTERFACE_REFERENCE InterfaceReference;PINTERFACE_DEREFERENCE InterfaceDereference;} INTERFACE,*PINTERFACE;
typedef struct {USHORT Size,Version;PVOID Context;PINTERFACE_REFERENCE InterfaceReference;PINTERFACE_DEREFERENCE InterfaceDereference;NTSTATUS(*GetPossibleBrightness)(PVOID,ULONG,PUCHAR,PUCHAR);NTSTATUS(*SetBrightness)(PVOID,UCHAR);NTSTATUS(*GetBrightness)(PVOID,PUCHAR);} DXGK_BRIGHTNESS_INTERFACE,*PDXGK_BRIGHTNESS_INTERFACE;
typedef struct {const GUID*InterfaceType;USHORT Size,Version;PINTERFACE Interface;PVOID InterfaceSpecificData;} QUERY_INTERFACE,*PQUERY_INTERFACE;
#endif
