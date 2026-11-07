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

## USB C0200211との照合

mainがUSB1004で確定したSTATUS_WDF_EXECUTION_LEVEL_INVALIDは、WdfWorkItemCreateの
既定validationがExecutionLevel明示を拒否する契約。本SD codeにはWdfWorkItemCreateがない。
SDの明示PassiveはWdfDeviceCreate(FDO/PDO)とWdfTimerCreateだけで、公開WDF sourceでは
両方ともFX_VALIDATE_OPTION_EXECUTION_LEVEL_ALLOWEDを指定する。
IoTargetCreateは親だけ指定しExecutionLevelはInheritのまま、IoQueueCreate/DriverCreateは
attributes無し。根拠なくPassiveを削除する「同型修正」は行わない。

## 初回class選択の別依存（Engine helper所有者へ）

読取り参照したEngine source88a7875のtools/winpe/engine-deferred-update.h:229–231は
SetupDiSetSelectedDriverW直後にDIF_INSTALLDEVICEを呼び、DIF_SELECTBESTCOMPATDRVはない。
一方、後boot自動installログではDIF_SELECTBESTCOMPATDRVのSelect Best Driverが
Class GUID変更を記録してからConfigure成功している。
Microsoft公式SetupDiSelectBestCompatDrvは選択driverとdeviceのclassが異なるときに
管理者としてdevice classをdriver classへ設定する契約を明示している。
初回NULL classからの選択工程の差は確認できるが、E0000204の直接因果は未実証。

Engine helperの変更は担当へ委ねる。修正時もsingle INF制限、hardware match、version/date/
CAT/hash pinを維持したcompatible driver listを使い、標準DIF_SELECTBESTCOMPATDRV後に
選択driverを再確認してからDIF_INSTALLDEVICEする。ClassInstall32新設やregistry class手書きで
隠さない。これは後bootSD Code31の修正とは別。

一次仕様:
https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdiselectbestcompatdrv

## 署名候補の回収

source16ed4f853788f6810f5a4644f94c638fa6f8d2e7、INF2026.11.7.4。
8TUを実WDK8.1 ARM32/NoFP/NEON flags/正規libcntprでcompile/linkし、SYS/CAT署名、
SYS/INF membership検証、root内回収の全manifest hash一致を確認した。
既存SD protocol fixtureの一括ASan/UBSan確認は通過。新しいmock/test fileは追加していない。

root内ZIP:
`port/xnu/out/windows-rt-sd-add-16ed4f8-signed/mt8167-sd-add-16ed4f8-signed.zip`

SHA256: `ade7c67557e2c13e54d4daeaa41482b89f47d8d6fd4f6311e7f69f74bb0c8d9c`

署名SYS SHA256: `f028660dc1bca998172f738bf023f5b8fbd8c4c3127e02d76d235db2069e0686`

親1010は再build/再署名していない。SD3も凍結維持。この候補を根因修正済み、CM成功、
card init成功、RW成功とは扱わない。実失敗stage/statusの次の確認はmainの実機観測が必要。
