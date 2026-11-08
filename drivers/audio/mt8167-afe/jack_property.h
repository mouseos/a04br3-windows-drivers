/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Filter-handle, pin-wise KSPROPSETID_Jack ABI (WDK 8.1).
 * This connector is a sense-only bridge, not a claimed headphone signal path. */
#ifndef MT8167_AFE_JACK_PROPERTY_H
#define MT8167_AFE_JACK_PROPERTY_H
#include "../../input/mt8167-accdet/socket.h"
#define MT8167_AFE_JACK_PIN 2u
static NTSTATUS Mt8167JackPropertyRead(PPCPROPERTY_REQUEST r,PDEVICE_OBJECT device,
    NTSTATUS (*read)(PDEVICE_OBJECT,struct mt8167_accdet_socket *))
{
    ULONG pin,needed,count,capacity;
    const ULONG access=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_BASICSUPPORT;
    if(!r)return STATUS_INVALID_PARAMETER;
    capacity=r->ValueSize;r->ValueSize=0;
    if(!r->PropertyItem||!r->PropertyItem->Set||
       !IsEqualGUIDAligned(*r->PropertyItem->Set,KSPROPSETID_Jack)||
       (r->PropertyItem->Id!=KSPROPERTY_JACK_DESCRIPTION&&
        r->PropertyItem->Id!=KSPROPERTY_JACK_DESCRIPTION2))return STATUS_INVALID_DEVICE_REQUEST;
    if(!r->Instance||r->InstanceSize<sizeof(pin))return STATUS_INVALID_PARAMETER;
    RtlCopyMemory(&pin,r->Instance,sizeof(pin));
    if(pin>MT8167_AFE_JACK_PIN)return STATUS_INVALID_PARAMETER;
    if(r->Verb==KSPROPERTY_TYPE_BASICSUPPORT) {
        needed=sizeof(ULONG);
        if(!capacity){r->ValueSize=needed;return STATUS_BUFFER_OVERFLOW;}
        if(capacity<needed){r->ValueSize=needed;return STATUS_BUFFER_TOO_SMALL;}
        if(!r->Value)return STATUS_INVALID_PARAMETER;
        if(capacity>=sizeof(KSPROPERTY_DESCRIPTION)) {
            KSPROPERTY_DESCRIPTION description;
            RtlZeroMemory(&description,sizeof(description));
            description.AccessFlags=access;description.DescriptionSize=sizeof(description);
            description.PropTypeSet.Set=KSPROPTYPESETID_General;
            description.PropTypeSet.Id=0; /* VT_EMPTY: structured property */
            RtlCopyMemory(r->Value,&description,sizeof(description));r->ValueSize=sizeof(description);
        }else{RtlCopyMemory(r->Value,&access,sizeof(access));r->ValueSize=sizeof(access);}
        return STATUS_SUCCESS;
    }
    if(r->Verb!=KSPROPERTY_TYPE_GET)return STATUS_INVALID_DEVICE_REQUEST;
    count=pin==MT8167_AFE_JACK_PIN?1u:0u;
    needed=sizeof(KSMULTIPLE_ITEM)+count*(r->PropertyItem->Id==KSPROPERTY_JACK_DESCRIPTION?
        sizeof(KSJACK_DESCRIPTION):sizeof(KSJACK_DESCRIPTION2));
    if(!capacity){r->ValueSize=needed;return STATUS_BUFFER_OVERFLOW;}
    if(capacity<needed){r->ValueSize=needed;return STATUS_BUFFER_TOO_SMALL;}
    if(!r->Value)return STATUS_INVALID_PARAMETER;
    if(count) {
        struct mt8167_accdet_socket sample;
        RtlZeroMemory(&sample,sizeof(sample));
        if(!device||!read)return STATUS_DEVICE_NOT_READY;
        NTSTATUS status=read(device,&sample);
        if(!NT_SUCCESS(status))return status;
        if(sample.size!=sizeof(sample)||sample.version!=1||sample.qualified!=1||sample.inserted>1)
            return STATUS_DEVICE_NOT_READY;
        if(r->PropertyItem->Id==KSPROPERTY_JACK_DESCRIPTION) {
            KSJACK_DESCRIPTION description;
            RtlZeroMemory(&description,sizeof(description));
            /* No guessed channel count, colour, diameter, side or mic type. */
            description.ConnectionType=eConnTypeUnknown;
            description.GeoLocation=eGeoLocNotApplicable;
            description.GenLocation=eGenLocPrimaryBox;
            description.PortConnection=ePortConnJack;
            description.IsConnected=sample.inserted?TRUE:FALSE;
            RtlCopyMemory((PUCHAR)r->Value+sizeof(KSMULTIPLE_ITEM),&description,sizeof(description));
        }else{
            KSJACK_DESCRIPTION2 description;
            RtlZeroMemory(&description,sizeof(description));
            description.JackCapabilities=JACKDESC2_PRESENCE_DETECT_CAPABILITY;
            RtlCopyMemory((PUCHAR)r->Value+sizeof(KSMULTIPLE_ITEM),&description,sizeof(description));
        }
    }
    KSMULTIPLE_ITEM multiple;multiple.Size=needed;multiple.Count=count;
    RtlCopyMemory(r->Value,&multiple,sizeof(multiple));r->ValueSize=needed;
    return STATUS_SUCCESS;
}
#endif
