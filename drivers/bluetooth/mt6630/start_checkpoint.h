/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded software lifecycle evidence. No hardware access or secret data.
 * Per-device ReachedMask prevents saved statuses from becoming current proof.
 */
#ifndef A04_MT6630_START_CHECKPOINT_H
#define A04_MT6630_START_CHECKPOINT_H
static void StartCheckpoint(WDFDEVICE device,ULONG *mask,ULONG stage,NTSTATUS status)
{
 static const PCWSTR names[]={L"DeviceCreated",L"WaitLock",L"IoTarget",L"Timer",L"Queues",L"ChildCreated",L"SidebandQI",L"ControllerQI",L"StaticChild",L"D0Entry",L"BrokerTargetOpen",L"BrokerHciOpen",L"OptionalController",L"D0Return",L"D0Exit",L"Capabilities",L"BrokerReceipt"};
 WDFKEY driver=NULL,key=NULL;WDF_OBJECT_ATTRIBUTES a;UNICODE_STRING name;NTSTATUS saved;
 if(!mask||stage>=RTL_NUMBER_OF(names)||KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 *mask|=1u<<stage;WDF_OBJECT_ATTRIBUTES_INIT(&a);
 saved=WdfDeviceOpenRegistryKey(device,PLUGPLAY_REGKEY_DRIVER,KEY_CREATE_SUB_KEY,&a,&driver);
 if(!NT_SUCCESS(saved))return;
 RtlInitUnicodeString(&name,L"MT6630StartCheckpoint");
 saved=WdfRegistryCreateKey(driver,&name,KEY_SET_VALUE,REG_OPTION_NON_VOLATILE,NULL,&a,&key);
 if(NT_SUCCESS(saved)){
  RtlInitUnicodeString(&name,names[stage]);(void)WdfRegistryAssignULong(key,&name,(ULONG)status);
  RtlInitUnicodeString(&name,L"ReachedMask");(void)WdfRegistryAssignULong(key,&name,*mask);
  RtlInitUnicodeString(&name,L"Schema");(void)WdfRegistryAssignULong(key,&name,1);
  WdfRegistryClose(key);
 }
 WdfRegistryClose(driver);
}
static void DriverStartCheckpoint(WDFDRIVER driver,PCWSTR label,NTSTATUS status)
{
 WDFKEY key=NULL;WDF_OBJECT_ATTRIBUTES a;UNICODE_STRING name;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);
 if(!NT_SUCCESS(WdfDriverOpenParametersRegistryKey(driver,KEY_SET_VALUE,&a,&key)))return;
 RtlInitUnicodeString(&name,label);(void)WdfRegistryAssignULong(key,&name,(ULONG)status);WdfRegistryClose(key);
}
static void BrokerStartCheckpoint(WDFDEVICE device,const struct a04_bt_start_status *receipt)
{
 WDFKEY driver=NULL,key=NULL;WDF_OBJECT_ATTRIBUTES a;UNICODE_STRING name;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||KeAreAllApcsDisabled())return;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);
 if(!NT_SUCCESS(WdfDeviceOpenRegistryKey(device,PLUGPLAY_REGKEY_DRIVER,KEY_CREATE_SUB_KEY,&a,&driver)))return;
 RtlInitUnicodeString(&name,L"MT6630StartCheckpoint");
 if(NT_SUCCESS(WdfRegistryCreateKey(driver,&name,KEY_SET_VALUE,REG_OPTION_NON_VOLATILE,NULL,&a,&key))){
  RtlInitUnicodeString(&name,L"BrokerOpenV1");(void)WdfRegistryAssignValue(key,&name,REG_BINARY,sizeof(*receipt),(PVOID)receipt);WdfRegistryClose(key);
 }
 WdfRegistryClose(driver);
}
#endif
