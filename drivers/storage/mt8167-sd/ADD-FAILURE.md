# SD3 trial47: 登録とAddDevice失敗の区別

SetupAPI後続bootはSD3/oem88.inf/Host.NTを選択し、サービス作成・copy・Configure成功後、
Starting deviceの約188msでCM_PROB_FAILED_ADD(31)となった。
Kernel-PnP record154/400はversion2026.11.7.3、record155/411はProblem0x1f/Status0。
Status0をdriver成功値や失敗DDIの特定と解釈しない。
初回DIFのE0000204はWDK setupapi.hでERROR_KEY_DOES_NOT_EXISTと一致するが、
後bootのCode31とは別工程。既存INFのSCSIAdapter classやWDF sectionが壊れたとは断定しない。

fresh48 SYSTEMはsequence1075/1074のdirty hiveだった。原本を書換えずLOG1/LOG2を
読取り適用するとMt8167Sdサービス、ClassGuid、WDF1.13登録が確認できた。
transaction log未反映でサービス不存在と判断しない。古いUSB1001イベントを現1004へ流用しない。

## この候補の範囲

SD専用code/checkpointとINF4だけ。親1010/SD3署名tuple、共通eMMC/Audio/USB/ACPIは非編集。
Code31 rootは未特定であり、この候補は実DDI返却statusを回収するための実装。
unsupportedを成功化せず、返すNTSTATUSは元のまま。専用I/O thread化は混ぜていない。

固定key: `HKLM\SYSTEM\CurrentControlSet\Services\Mt8167Sd\Parameters\NativeCheckpoint`

固定REG_BINARY値は `DriverEntry`, `AddDevice`, `PrepareHardware`, `D0Entry` の4個。
各40bytes、LE:

| offset | type | 内容 |
| --- | --- | --- |
| 0 | ULONG | size=40 |
| 4 | ULONG | schema=1 |
| 8 | ULONG | driver_revision=4 |
| 12 | ULONG | sequence: このload中の記録順 |
| 16 | ULONG | stage |
| 20 | ULONG | 実NTSTATUS |
| 24 | ULONG | complete: 0=未実行/入口、1=callback帰還 |
| 28 | ULONG | reserved=0 |
| 32 | ULONGLONG | KeQueryInterruptTime、100ns |

DriverEntryの始めに後続3値をstage0/complete0に戻す。Add入口はstage0x100/complete0。
DriverEntry stage1=入口、stage2=WdfDriverCreate帰還。
PrepareHardware/D0Entry stage1=入口、stage2=帰還。
AddDeviceの帰還stage:

| stage | 最後に呼んだDDI |
| --- | --- |
| 1 | FDO PnP preprocess登録 |
| 2 | FDO WdfDeviceCreate |
| 3 | WdfTimerCreate |
| 20 | WdfPdoInitAllocate |
| 21 | AssignDeviceID |
| 22 | AddHardwareID |
| 23 | AddCompatibleID |
| 24 | AssignInstanceID |
| 25 | AddDeviceText |
| 26 | child SCSI preprocess登録 |
| 27 | child FLUSH preprocess登録 |
| 28 | child WdfDeviceCreate |
| 30 | child WdfIoQueueCreate |
| 31 | WdfFdoAddStaticChild |

complete1かつstatus失敗のAdd stageで実失敗を特定する。DriverEntry/Addとも成功していても
Prepare/D0未到達ならPnPの別経路を調べる。記録無しはloader以前の失敗のほか、key作成/書込
失敗もあり得るため、DriverEntry未実行とは断定しない。ZwSetValueKeyを回収手段として使うが、
ZwFlushKeyや同期UARTは呼ばず、記録のdurabilityやハード成功を保証したとは扱わない。

## 待ちのsource確認（Code31原因とは未確定）

SD3はStorport miniportでなくKMDF host+GenDisk PDO。CMD/PIOは1要求共通の絶対deadline
（既定5秒、SRB指定でも上限10秒）。初期化失敗後はpoison/generationで再試行を抑える。
D0EntryのSDCF/SDCD ACPI送信は各1秒、resetは200ms。
ただし同期ACPIやsupplier QueryInterface/StopIdle/共通parent mutexに関して、下位driverが
正常に完了する前提を超える絶対上限までは証明しない。

既存最大8system work itemが同一serial mutexを無期限に待てる。descriptor queryにも
無期限serial待ちがあり、power exitはrundownとmutexを待つ。共用worker占有リスクは実在するが、
今回のSD Add失敗を通過してSRBが動いた証拠はない。SCM数分遅延のrootとは断定しない。
partial install→サービスdisabled+3010により次bootの診断が無い経路を先に区別する。

一次仕様の照合はMicrosoft Windows-Driver-Frameworks source
b6191d9543441329154da32f7ab9bdd97228dd3cとDDI docsで実施した。
現代の公開WDF sourceを対象実機WDF1.13の動作証拠とは扱わない。
