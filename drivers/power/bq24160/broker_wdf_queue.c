/* SPDX-License-Identifier: MIT */
#include "broker_wdf.h"
typedef struct { struct a04_broker_endpoint *endpoint; WDFWAITLOCK lock; a04_broker_wdf_failure_fn failure; void *failure_context; } BROKER_QUEUE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BROKER_QUEUE_CONTEXT,BrokerQueueContext)
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL BrokerInternalControl;
VOID BrokerInternalControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code)
{
    BROKER_QUEUE_CONTEXT *c=BrokerQueueContext(queue);
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    if (a04_broker_wdf_receive(c->endpoint,c->lock,request,code) && c->failure)
        c->failure(c->failure_context);
}
NTSTATUS a04_broker_wdf_register_receiver(WDFDEVICE device,struct a04_broker_endpoint *endpoint,
                                          WDFWAITLOCK lock,a04_broker_wdf_failure_fn failure,void *failure_context,WDFQUEUE *out)
{
    WDF_IO_QUEUE_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDFQUEUE queue;
    BROKER_QUEUE_CONTEXT *context;
    NTSTATUS status;
    if (!device || !endpoint || !lock || !failure || !failure_context || !out)
        return STATUS_INVALID_PARAMETER;
    *out=NULL;
    WDF_IO_QUEUE_CONFIG_INIT(&config,WdfIoQueueDispatchSequential);
    config.EvtIoInternalDeviceControl=BrokerInternalControl;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs,BROKER_QUEUE_CONTEXT);
    attrs.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfIoQueueCreate(device,&config,&attrs,&queue);
    if (!NT_SUCCESS(status)) return status;
    /* A non-default queue is not dispatched until configured below. */
    context=BrokerQueueContext(queue);context->endpoint=endpoint;context->lock=lock;
    context->failure=failure;context->failure_context=failure_context;
    status=WdfDeviceConfigureRequestDispatching(device,queue,WdfRequestTypeDeviceControlInternal);
    if (!NT_SUCCESS(status)) { WdfObjectDelete(queue);return status; }
    *out=queue;return STATUS_SUCCESS;
}
