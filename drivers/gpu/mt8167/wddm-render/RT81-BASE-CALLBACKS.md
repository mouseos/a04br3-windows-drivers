# Windows RT 8.1 ARM base callback contract

実機run65はDriverEntry/Contract成功、DxgkInitialize=0xC0000059、AddDevice未到達。
REAで実機dxgkrnl.sys 6.3.9600.16455（SHA256 2f0fe1db882ba83862a6ece3c2de0cf08c2b20767e6640dc4de5b97255b6d941）を調べた。
IOCTL 0x230047の返す0x1cb25は、初期化tableの1..11、13、14番目のNULLを
STATUS_REVISION_MISMATCHで拒否する。版番号0x4002だけが原因とは観測していない。
REA evidence ev_2b29783e860580eb51c93201466f4ba483a3ac1a4b44a9e2254eb88e922475c3。

3.6はDisplay-specificな実表示機能を広告しない。MOU0006はsource/child=0を維持し、
必要なbase callbacksに「出力無し」の実状態を応答する。ChildRelationsは規定のzero terminator、
ChildUid/source要求はinvalid、EDIDは生成しない。DispatchIoRequestはVP_STATUSとInformationも設定。
ResetDeviceはVGA復帰専用でAny IRQL。MOU0006はVGA/POST/scanoutを所有しないため
MOU0007のdisplayへ触れず、PASSIVE-onlyのGPU TDR resetを流用しない。
UnloadはDriverEntryの非確保状態とcheckpointのみをcleanupする。各deviceの資源寿命は従来のRemoveDevice。

Microsoft一次資料:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_query_child_relations
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_dispatch_io_request
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_reset_device
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_unload

これは実機kernelのbase gateへの対応であり、全WDDM/FL9_1適合や描画成功を意味しない。
Microsoft LINKを保持、UMD/FW/native ownerは不変、miniport.objだけを増分compileする。
実機の初期化・標準API描画は未確認。
