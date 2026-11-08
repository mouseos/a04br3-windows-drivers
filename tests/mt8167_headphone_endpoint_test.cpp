/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define A04_MT8167_WAVERT_EXPERIMENTAL 1
#define A04_AUDIO_ENGINE_PCM 1
#define A04_AUDIO_CAPTURE 1
#include <cassert>
#include <vector>
#include <cstdio>
#include "../drivers/audio/mt8167-afe/headphone_endpoint.cpp"
const GUID IID_IUnknown={1},IID_IMiniport={2},IID_IMiniportTopology={3};
const GUID KSDATAFORMAT_TYPE_AUDIO={4},KSDATAFORMAT_SUBTYPE_ANALOG={5},KSDATAFORMAT_SPECIFIER_NONE={6},KSCATEGORY_AUDIO={7},KSNODETYPE_HEADPHONES={8},KSPROPSETID_Jack={9},KSPROPTYPESETID_General={10};
const int CLSID_PortWaveRT=1,CLSID_PortTopology=2;
static int calls=0,fail=0,providerRefs=0,qualifiedResult=0,jackResult=0;
static ULONG inserted=1,jackVersion=1;static std::vector<PPORT> registered;
static Mt8167HeadphoneTopology *topoObject;
static const Mt8167HeadphoneProvider *currentProvider;
int KeGetCurrentIrql(){return 0;}
static void ref(void*){++providerRefs;}static void deref(void*){--providerRefs;}
static NTSTATUS ready(void*){return qualifiedResult;}
static NTSTATUS jack(void*,mt8167_accdet_socket*s){*s={16,jackVersion,1,inserted};return jackResult;}
static bool fault(){return ++calls==fail;}
struct Mini:IMiniportWaveRT{
 LONG refs=1;NTSTATUS QueryInterface(REFIID,PVOID*)override{return -1;}ULONG AddRef()override{return ++refs;}ULONG Release()override{LONG n=--refs;if(!n)delete this;return n;}
 NTSTATUS GetDescription(PPCFILTER_DESCRIPTOR*)override{return 0;}NTSTATUS DataRangeIntersection(ULONG,PKSDATARANGE,PKSDATARANGE,ULONG,PVOID,PULONG)override{return 0;}
 NTSTATUS Init(PUNKNOWN,PRESOURCELIST,PPORTWAVERT)override{return 0;}NTSTATUS NewStream(PMINIPORTWAVERTSTREAM*,PPORTWAVERTSTREAM,ULONG,BOOLEAN,PKSDATAFORMAT)override{return 0;}NTSTATUS GetDeviceDescription(PDEVICE_DESCRIPTION)override{return 0;}
};
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT*out,const Mt8167StreamBackend*b){assert(b->context&&b->reference&&b->dereference&&!b->capture);*out=nullptr;if(fault())return STATUS_INSUFFICIENT_RESOURCES;*out=new Mini;return 0;}
static NTSTATUS makeWave(void *context,PMINIPORTWAVERT *out){assert(context==currentProvider->stream.context);return Mt8167NewMiniportWithBackend(out,&currentProvider->stream);}
struct Port:IPort{
 int type;LONG refs=1;PUNKNOWN mini=nullptr;
 explicit Port(int t):type(t){}
 NTSTATUS Init(PDEVICE_OBJECT,PIRP,PUNKNOWN m,PUNKNOWN,PRESOURCELIST)override{if(fault())return -50;mini=m;if(type==1)static_cast<Mini*>(mini)->AddRef();else{topoObject=static_cast<Mt8167HeadphoneTopology*>(mini);topoObject->AddRef();}return 0;}
 ULONG Release()override{LONG n=--refs;if(!n){if(mini){if(type==1)static_cast<Mini*>(mini)->Release();else static_cast<Mt8167HeadphoneTopology*>(mini)->Release();}delete this;}return n;}
};
NTSTATUS PcNewPort(PPORT*p,const int&id){*p=nullptr;if(fault())return -50;*p=new Port(id);return 0;}
NTSTATUS PcRegisterSubdevice(PDEVICE_OBJECT,WCHAR*name,PUNKNOWN p){assert(name);if(fault())return -50;auto*port=static_cast<Port*>(p);port->refs++;registered.push_back(port);return 0;}
NTSTATUS PcRegisterPhysicalConnection(PDEVICE_OBJECT,PUNKNOWN,ULONG wavePin,PUNKNOWN,ULONG topoPin){assert(wavePin==1&&topoPin==0);return fault()?-50:0;}
static void unwind(){for(auto p:registered)p->Release();registered.clear();topoObject=nullptr;assert(!providerRefs);}
int main(){
 Mt8167HeadphoneProvider p={};int owner=0;p.stream.context=&owner;p.stream.reference=ref;p.stream.dereference=deref;p.qualified=ready;p.readJack=jack;p.newWaveMiniport=makeWave;currentProvider=&p;
 auto run=[&](){return Mt8167RegisterHeadphoneWaveRt(&owner,&owner,&owner,&p);};
 qualifiedResult=STATUS_DEVICE_NOT_READY;assert(run()==STATUS_DEVICE_NOT_READY&&!calls&&!providerRefs);qualifiedResult=1;assert(run()==STATUS_DEVICE_NOT_READY&&!calls);qualifiedResult=0;
 p.readJack=nullptr;assert(run()==STATUS_DEVICE_NOT_READY&&!calls);p.readJack=jack;p.stream.capture=TRUE;assert(run()==STATUS_INVALID_PARAMETER&&!calls);p.stream.capture=FALSE;
 assert(run()==0&&registered.size()==2&&providerRefs==1);int total=calls;
 PPCFILTER_DESCRIPTOR descriptor=nullptr;assert(topoObject->GetDescription(&descriptor)==0&&descriptor->pinCount==2&&descriptor->connectionCount==1&&descriptor->pins[1].pin.category==&KSNODETYPE_HEADPHONES);
 alignas(8) unsigned char output[128];ULONG pin=1;PCPROPERTY_REQUEST r={topoObject,&Properties[0],1,sizeof(pin),&pin,sizeof(output),output};
 assert(HeadphoneProperty(&r)==0&&r.ValueSize==36);auto*d=(KSJACK_DESCRIPTION*)(output+8);assert(d->IsConnected==1&&d->ChannelMapping==3);
 inserted=0;r.ValueSize=sizeof(output);assert(HeadphoneProperty(&r)==0&&d->IsConnected==0);
 qualifiedResult=STATUS_DEVICE_NOT_READY;r.ValueSize=sizeof(output);assert(HeadphoneProperty(&r)==STATUS_DEVICE_NOT_READY&&!r.ValueSize);qualifiedResult=0;
 jackVersion=2;r.ValueSize=sizeof(output);assert(HeadphoneProperty(&r)==STATUS_DEVICE_NOT_READY&&!r.ValueSize);jackVersion=1;
 pin=0;r.ValueSize=sizeof(output);assert(HeadphoneProperty(&r)==0&&r.ValueSize==8&&((KSMULTIPLE_ITEM*)output)->Count==0);pin=2;r.ValueSize=sizeof(output);assert(HeadphoneProperty(&r)==STATUS_INVALID_PARAMETER);pin=1;
 r.PropertyItem=&Properties[1];r.ValueSize=16;assert(HeadphoneProperty(&r)==0&&((KSJACK_DESCRIPTION2*)(output+8))->JackCapabilities==1);
 unwind();for(fail=1;fail<=total;fail++){calls=0;assert(run()<0);unwind();}
 puts("HP endpoint: no-provider rejection, standard graph/property, partial-registration unwind PASS");
}
