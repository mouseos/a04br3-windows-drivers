#ifndef A04_NATIVE_SCAN_TEST_NDIS_H
#define A04_NATIVE_SCAN_TEST_NDIS_H
#include <stdint.h>
#include <string.h>
typedef void *NDIS_HANDLE;
typedef uint32_t ULONG;typedef uint8_t UCHAR;typedef uint8_t BOOLEAN;typedef int32_t NDIS_STATUS;
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_INVALID_DATA (-1)
#define NDIS_STATUS_INVALID_LENGTH (-2)
#define NDIS_STATUS_NOT_SUPPORTED (-3)
#define NDIS_STATUS_FAILURE (-4)
#define NDIS_STATUS_DOT11_MEDIA_IN_USE (-5)
#define NDIS_STATUS_DOT11_SCAN_CONFIRM 0x1234
#define NDIS_OBJECT_TYPE_STATUS_INDICATION 0x80
#define NDIS_STATUS_INDICATION_REVISION_1 1
#define NDIS_SIZEOF_STATUS_INDICATION_REVISION_1 ((uint16_t)sizeof(NDIS_STATUS_INDICATION))
typedef struct {UCHAR Type,Revision;uint16_t Size;} NDIS_OBJECT_HEADER;
typedef struct {NDIS_OBJECT_HEADER Header;NDIS_HANDLE SourceHandle;NDIS_STATUS StatusCode;void *StatusBuffer;ULONG StatusBufferSize;} NDIS_STATUS_INDICATION;
void NdisMIndicateStatusEx(NDIS_HANDLE,NDIS_STATUS_INDICATION *);
#define NdisZeroMemory(a,b) memset((a),0,(b))
#define NdisMoveMemory(a,b,c) memmove((a),(b),(c))
enum {dot11_BSS_type_infrastructure=1,dot11_BSS_type_independent=2,dot11_BSS_type_any=3};
enum {dot11_scan_type_active=1,dot11_scan_type_passive=2,dot11_scan_type_auto=3};
typedef struct {ULONG uSSIDLength;UCHAR ucSSID[32];} DOT11_SSID;
typedef struct {ULONG dot11BSSType;UCHAR dot11BSSID[6];ULONG dot11ScanType;BOOLEAN bRestrictedScan;ULONG udot11SSIDsOffset,uNumOfdot11SSIDs;BOOLEAN bUseRequestIE;ULONG uRequestIDsOffset,uNumOfRequestIDs,uPhyTypeInfosOffset,uNumOfPhyTypeInfos,uIEsOffset,uIEsLength;UCHAR ucBuffer[1];} DOT11_SCAN_REQUEST_V2;
#endif
