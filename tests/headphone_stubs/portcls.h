#pragma once
#define A04_TEST_MINIPORT 1
#include "../audio_windows_stubs/portcls.h"
#include <new>
typedef wchar_t WCHAR;typedef void *PDEVICE_OBJECT,*PIRP;typedef unsigned char *PUCHAR;
#define RtlCopyMemory(p,s,n) memcpy(p,s,n)
#define PASSIVE_LEVEL 0
#define STATUS_INVALID_DEVICE_STATE (-17)
#define STATUS_INVALID_DEVICE_REQUEST (-18)
#define KSPROPERTY_TYPE_GET 1u
#define KSPROPERTY_TYPE_BASICSUPPORT 0x200u
#define KSPROPERTY_JACK_DESCRIPTION 1
#define KSPROPERTY_JACK_DESCRIPTION2 2
#define JACKDESC2_PRESENCE_DETECT_CAPABILITY 1u
#define SPEAKER_FRONT_LEFT 1u
#define SPEAKER_FRONT_RIGHT 2u
enum {eConnTypeUnknown,eGeoLocNotApplicable,eGenLocPrimaryBox,ePortConnJack};
struct KSMULTIPLE_ITEM{ULONG Size,Count;};
struct KSJACK_DESCRIPTION{ULONG ChannelMapping,Color,ConnectionType,GeoLocation,GenLocation,PortConnection;LONG IsConnected;};
struct KSJACK_DESCRIPTION2{ULONG DeviceStateInfo,JackCapabilities;};
struct KSPROPERTY_DESCRIPTION{ULONG AccessFlags,DescriptionSize;struct {GUID Set;ULONG Id,Flags;}PropTypeSet;ULONG MembersListCount,Reserved;};
struct PCPROPERTY_REQUEST;typedef PCPROPERTY_REQUEST *PPCPROPERTY_REQUEST;
struct PCPROPERTY_ITEM{const GUID *Set;ULONG Id,Flags;NTSTATUS(*Handler)(PPCPROPERTY_REQUEST);};
struct PCPROPERTY_REQUEST{PVOID MajorTarget;PCPROPERTY_ITEM *PropertyItem;ULONG Verb,InstanceSize;PVOID Instance;ULONG ValueSize;PVOID Value;};
#define DEFINE_PCAUTOMATION_TABLE_PROP(name,items) static int name
extern const GUID KSPROPSETID_Jack,KSPROPTYPESETID_General,KSNODETYPE_HEADPHONES;
extern const int CLSID_PortWaveRT,CLSID_PortTopology;
struct IPort {virtual NTSTATUS Init(PDEVICE_OBJECT,PIRP,PUNKNOWN,PUNKNOWN,PRESOURCELIST)=0;virtual ULONG Release()=0;};
typedef IPort *PPORT;
NTSTATUS PcNewPort(PPORT *,const int&);
NTSTATUS PcRegisterSubdevice(PDEVICE_OBJECT,WCHAR*,PUNKNOWN);
NTSTATUS PcRegisterPhysicalConnection(PDEVICE_OBJECT,PUNKNOWN,ULONG,PUNKNOWN,ULONG);
int KeGetCurrentIrql();
