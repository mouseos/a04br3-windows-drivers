/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded software lifecycle evidence. No hardware access or secret data.
 * Per-device ReachedMask prevents saved statuses from becoming current proof.
 */
#ifndef A04_MT6630_START_CHECKPOINT_H
#define A04_MT6630_START_CHECKPOINT_H
static void StartCheckpoint(WDFDEVICE device,ULONG *mask,ULONG stage,NTSTATUS status)
{
 static const PCWSTR names[]={L"DeviceCreated",L"WaitLock",L"IoTarget",L"Timer",L"Queues",L"ChildCreated",L"SidebandQI",L"ControllerQI",L"StaticChild",L"D0Entry",L"BrokerTargetOpen",L"BrokerHciOpen",L"OptionalController",L"D0Return",L"D0Exit"};
 WDFKEY driver=NULL,key=NULL;WDF_OBJECT_ATTRIBUTES a;UNICODE_STRING name;NTSTATUS saved;
 if(!mask||stage>=RTL_NUMBER_OF(names)||KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
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
#endif
