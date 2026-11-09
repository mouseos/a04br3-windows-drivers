# MT6630 Bluetooth client2014 開始経路修正候補

最新combo1023 source701182c1を基点とし、既署名client2013 sourcef27cbdd7のclient側27ファイルだけを追加した。既存provider transport6ファイルは同byte、network sourceは無変更。provider1023/親/Audio/firmwareを再実装・再buildしない。

## 根拠と修正

回収済みSystem.evtxにはROOT\SYSTEM\0001 / Mt6630ComboBluetoothのAddServiceStatus0と、Kernel-PnP219 Status0xC000025E (STATUS_PLUGPLAY_NO_DEVICE) がある。後続actualのroot1/child0/Code10/radio21と合わせても、失敗APIやSTP command段階までは特定できない。root登録初回失敗やWLAN profile0/5023を原因に読み替えない。

client2013のchild()はa04_combo_controller_init()の成功を静的HCI子の追加条件にし、そのinitは任意HFP通知用IoRegisterDeviceInterfaceを行っていた。この登録失敗だけでHCI子作成を止める依存はsourceから確定する。新候補はAddDeviceではsoftware QUERY_INTERFACEだけを作り、通知登録を実broker HCI OPEN/初期化とphysical owner参照取得の後へ移す。登録または公開失敗は本来のNTSTATUSを返して参照を解放し、controller ready/Audio leaseを与えない。HCI D0は従来どおり実broker OPENが成功した場合のみ進む。

これは任意通知と実transportの寿命を分離する修正であり、219が当該登録から返ったことの証明ではない。旧候補をそのまま再試験する依頼ではなく、新しい機能差分の候補。実Code10解消/標準radio/RF通信は未検証。

StartCheckpointは選択driver key配下MT6630StartCheckpointに固定15stage/DWORD status、Schema1、ReachedMaskを記録する。BD_ADDR/NVRAM/command payload/アドレスや任意registry pathは保存しない。D0開始ごとにD0関連maskをclearし、保存された未到達statusを現成功としない。記録失敗は本来のWDF/transport戻り値を変えない。登録/実機I/Oの再実行を診断から行わない。

## 一次source

- CT3 kernel-4.4/drivers/misc/mediatek/connectivity/bt/stp_chrdev_bt.c: BT_openはWMT BT function-on/STP ready、通常read/writeはBT_TASK。HFP Audio endpoint/通知登録とは独立。
- Linux a04br3_mt6630_bt.c: HCI function/setupとstock NVRAM vendor command列を分離。今回vendor値/PCM/clock/電源処理に変更なし。
- XNU由来vendor_xnu/A04BR3WMTRoundtrip.h、A04BR3WMTDownload.h: 既存fn2 STP bootstrap/credit ownerをそのまま保持。Windows SDIOは既存NDIS単一workerのみ。
- Microsoft Windows-driver-samples/bluetooth/serialhcibus Fdo.c/Pdo.c: radio transport、子PDO、音声側契約を区別。
- Microsoft Windows-Driver-Frameworks fxiotargetremote.cpp/fxiotargetremotekm.cpp: remote targetのPnP通知/TargetDeviceRelation失敗だけはopen成功へ正規化される。このため「non-PnP NDIS targetなら必ずWdfIoTargetOpenが失敗する」という仮説は採用しない。
- Microsoft IoRegisterDeviceInterface/WdfIoTargetOpenの公開契約を参照。実RT8.1内部traceを得たとは主張しない。バイナリ逆解析は実施していない。

## 保持・検証

root HWID/service、MS_BTHX_BTHMINI child、broker ABI、transport Bypass/1の実predicate、SCO/latency/drain/PCM資格を保持。実HFP providerの未知値を合成しない。root再登録、新MMIO、SDIO/PMIC/clock/firmware command追加なし。

production combo_controller.cを直接実行するhost testで、AddDevice時に通知登録しないこと、実owner前に登録しないこと、登録/enable失敗時の参照解放、失敗後再開始、hold中stop拒否をASan/UBSan確認する。WDF/owner mocksであり実PnP/RF結果ではない。

build入口tools/ci/build-mt6630-client2014-start.ps1。既client小closureのhfp_target/radio_binding2objectをsource/header hash一致で再利用、combo_client/combo_controller2TUのみcompile/link/sign。INF2014/表示名MediaTek MT6630 Bluetooth。成果物はmain次通常batchの採用判断用。GPU343/155媒体や実機には触れない。
