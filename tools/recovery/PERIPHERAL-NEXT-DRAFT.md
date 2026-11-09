# 次の周辺一括profile（main承認fresh122以降）

profile `peripheral-usb1010-wifi1020-sd8-dod17-parent1102-next-v1`、専用service/data directory `MT8167PeripheralNext122`。
旧MT8167PeripheralBatch/RenderSingleGpu/Engineサービスと全ledgerはbounded native snapshotの前後一致で保持し、既存serviceを再armしない。

stage順RTC/thermal→USB1010→WiFi1020→SD8→DOD1.7→親ACCDET1102。
RTC/thermalはstore-only、5対象だけDIF_INSTALLDEVICE、live restartなし、計画reboot最大1回。
新親は1101の全53資格と8range/既存Audio/capture等を保持する署名済み1102。
eMMC AJTD4R1.9/MMA1.10/GPU/UMD/FW/client/Engine/旧logs/旧SOFTWARE/userfs/GPTは保持し、同候補の再投入/再benchmarkをしない。
thermal観測boot125f42b5 / FD bf2c5f37は機種source8e2597c3の完成artifactを再利用し、main管理のmatched8range handoffを要求する。共通EDK変更/専用追加boot試験はしない。

source準備:
- generated target数は上限9、明示install mask0x7cだけDIF。従来6target profileは0x3cを保持。
- CM rc/started/Problemをmetadata guardより先に独立記録。選択版/date/SYSの失敗も別記録し、metadata失敗をstarted成功へ変換しない。
- DEVPKEY version/dateがERROR_NOT_FOUNDの時だけ同devnodeのDIREG_DRVへfallback。型/byte数/値/INF/service/class/SYS/CAT比較を緩和しない。
- native readerのIncludeWifiSd/IncludePreviousBatchは固定6target/旧6serviceだけを追加。原trioはloadせず新cloneでreplayし、main指定manifest/policy/boot、原trio不変、unload/権限復元0を要求。
- 明示main approval receiptのhash、capture122+のmanifest/hash/boot/GPU policy、同capture native/seven/current GPU independent sealを一体検証。fresh120は参考資料のみ。sourceに未確定fresh hashを埋めず、未結合draftからservice/header/hive/mediaを生成しない。

音声:
搭載audio4facb36e/source2d623172の保存NativeStartはrevision1026/READY(14)/status0、DMAadapter/operations=1、map registers64。保持capture.logはbuffer16KiB、STOP-buffer→ACQUIRE/PAUSE/RUN/STOPの2pass、各CUR変化19/IRQ増分7、end0/aggregate非0を記録している。過去の限定capture実観測であり、fresh122/currentbootや標準Audio Engine・物理マイク・可聴再生の成功とはしない。
よってDMAadapter欠如を補う架空adapter/MDL代替や、同capture再build/再実行は行わない。旧capture.logは保存し、capture_clientは既定None。sourceのログ分離helperは未採用・未buildのdefault-off候補として保持する。
同bootでは署名ACCDET --readを1process、DOD4mode wrapperを1processだけ実行する。wrapper/session/user3EXEを固定MT8167DisplayRotationProbeV1へstageし、外側timeout300秒。元mode復元は既存Carson clientのfinally/監督契約へ任せ、非0/unsupported/資格不足を成功化しない。SCO/BT能力やlatency/drain/校正/analog routeは変更しない。実姿勢/物理タッチ/物理表示成功とAPI/DDI受理を分離する。

`prepare-peripheral-next-draft.py` は既存署名7packageを一つのdraftに集約する。
`peripheral_next_policy.py` / `build-peripheral-next.ps1` / media --next-profileが最終入口。
現時点で最終SCM/native arm/媒体は未生成。実機owner main、workerは実機/UART操作なし。
