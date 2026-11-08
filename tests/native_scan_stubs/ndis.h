#ifndef A04_NATIVE_SCAN_TEST_NDIS_H
#define A04_NATIVE_SCAN_TEST_NDIS_H
#include <stdint.h>
#include <string.h>
typedef void *NDIS_HANDLE;typedef void *PVOID;
typedef uint32_t ULONG;typedef int32_t LONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;typedef uint8_t BOOLEAN;typedef int32_t NDIS_STATUS;
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_INVALID_DATA (-1)
#define NDIS_STATUS_INVALID_LENGTH (-2)
#define NDIS_STATUS_NOT_SUPPORTED (-3)
#define NDIS_STATUS_FAILURE (-4)
#define NDIS_STATUS_DOT11_MEDIA_IN_USE (-5)
#define NDIS_STATUS_RESOURCES (-6)
#define NDIS_STATUS_PENDING 1
#define NDIS_STATUS_REQUEST_ABORTED (-7)
#define NDIS_STATUS_ADAPTER_NOT_READY (-8)
typedef void VOID;
typedef struct {int held;} NDIS_SPIN_LOCK;
typedef struct {int signaled;} NDIS_EVENT;
typedef VOID (NDIS_IO_WORKITEM_FUNCTION)(PVOID,NDIS_HANDLE);
void NdisAllocateSpinLock(NDIS_SPIN_LOCK *);void NdisFreeSpinLock(NDIS_SPIN_LOCK *);
void NdisAcquireSpinLock(NDIS_SPIN_LOCK *);void NdisReleaseSpinLock(NDIS_SPIN_LOCK *);
void NdisInitializeEvent(NDIS_EVENT *);void NdisSetEvent(NDIS_EVENT *);void NdisResetEvent(NDIS_EVENT *);int NdisWaitEvent(NDIS_EVENT *,unsigned);
NDIS_HANDLE NdisAllocateIoWorkItem(NDIS_HANDLE);void NdisFreeIoWorkItem(NDIS_HANDLE);
void NdisQueueIoWorkItem(NDIS_HANDLE,NDIS_IO_WORKITEM_FUNCTION *,PVOID);
#define OID_DOT11_SCAN_REQUEST 0x0d01020bu
#define NdisRequestSetInformation 1
typedef struct {int RequestType;PVOID RequestId;union {struct {ULONG Oid;void *InformationBuffer;ULONG InformationBufferLength,BytesRead,BytesNeeded;} SET_INFORMATION;} DATA;} NDIS_OID_REQUEST,*PNDIS_OID_REQUEST;
void NdisMOidRequestComplete(NDIS_HANDLE,PNDIS_OID_REQUEST,NDIS_STATUS);
typedef struct _NET_BUFFER {struct _NET_BUFFER *next;ULONG length;UCHAR *data;int fragmented;} NET_BUFFER,*PNET_BUFFER;
typedef struct _NET_BUFFER_LIST {struct _NET_BUFFER_LIST *next;PNET_BUFFER first;NDIS_STATUS status;void *info,*priority;} NET_BUFFER_LIST,*PNET_BUFFER_LIST;
#define NET_BUFFER_LIST_NEXT_NBL(x) ((x)->next)
#define NET_BUFFER_LIST_FIRST_NB(x) ((x)->first)
#define NET_BUFFER_NEXT_NB(x) ((x)->next)
#define NET_BUFFER_DATA_LENGTH(x) ((x)->length)
#define NET_BUFFER_LIST_STATUS(x) ((x)->status)
#define NET_BUFFER_LIST_INFO(x,id) (*((id)==0?&(x)->info:&(x)->priority))
#define Ieee8021QNetBufferListInfo 1
typedef union {void *Value;struct {uintptr_t UserPriority:3,CanonicalFormatId:1,VlanId:12,Reserved:16;} TagHeader;} NDIS_NET_BUFFER_LIST_8021Q_INFO;
#define MediaSpecificInformation 0
#define DOT11_MAX_NUM_OF_FRAGMENTS 16
#define DOT11_EXTSTA_RECV_CONTEXT_REVISION_1 1
#define NDIS_OBJECT_TYPE_DEFAULT 0x80
#define NDIS_RECEIVE_FLAGS_RESOURCES 1
void *NdisGetDataBuffer(PNET_BUFFER,ULONG,void *,ULONG,ULONG);
void NdisMSendNetBufferListsComplete(NDIS_HANDLE,PNET_BUFFER_LIST,ULONG);
#define NDIS_STATUS_DOT11_SCAN_CONFIRM 0x1234
#define NDIS_OBJECT_TYPE_STATUS_INDICATION 0x80
#define NDIS_STATUS_INDICATION_REVISION_1 1
#define NDIS_SIZEOF_STATUS_INDICATION_REVISION_1 ((uint16_t)sizeof(NDIS_STATUS_INDICATION))
typedef struct {UCHAR Type,Revision;uint16_t Size;} NDIS_OBJECT_HEADER;
typedef struct {NDIS_OBJECT_HEADER Header;NDIS_HANDLE SourceHandle;NDIS_STATUS StatusCode;void *StatusBuffer;ULONG StatusBufferSize;} NDIS_STATUS_INDICATION;
void NdisMIndicateStatusEx(NDIS_HANDLE,NDIS_STATUS_INDICATION *);
typedef struct {NDIS_OBJECT_HEADER Header;ULONG uReceiveFlags,uPhyId,uChCenterFrequency;USHORT usNumberOfMPDUsReceived;LONG lRSSI;UCHAR ucDataRate;ULONG uSizeMediaSpecificInfo;void *pvMediaSpecificInfo;uint64_t ullTimestamp;} DOT11_EXTSTA_RECV_CONTEXT;
void NdisMIndicateReceiveNetBufferLists(NDIS_HANDLE,PNET_BUFFER_LIST,ULONG,ULONG,ULONG);
#define NdisZeroMemory(a,b) memset((a),0,(b))
#define NdisMoveMemory(a,b,c) memmove((a),(b),(c))
enum {dot11_BSS_type_infrastructure=1,dot11_BSS_type_independent=2,dot11_BSS_type_any=3};
enum {dot11_scan_type_active=1,dot11_scan_type_passive=2,dot11_scan_type_auto=3};
typedef struct {ULONG uSSIDLength;UCHAR ucSSID[32];} DOT11_SSID;
typedef struct {ULONG dot11BSSType;UCHAR dot11BSSID[6];ULONG dot11ScanType;BOOLEAN bRestrictedScan;ULONG udot11SSIDsOffset,uNumOfdot11SSIDs;BOOLEAN bUseRequestIE;ULONG uRequestIDsOffset,uNumOfRequestIDs,uPhyTypeInfosOffset,uNumOfPhyTypeInfos,uIEsOffset,uIEsLength;UCHAR ucBuffer[1];} DOT11_SCAN_REQUEST_V2;
#endif
