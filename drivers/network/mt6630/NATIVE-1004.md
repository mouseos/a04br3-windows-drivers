# Native 1004: 到達不能閾値と省電力値の確定

署名済み1bd250c/INF1003は保持する。この後続はINF
`11/06/2026,2026.11.6.1004` とし、旧DriverStore再利用と混同しない。
WiFi以外、親ABI、firmware/MMIO仕様は変更しない。実機/runner/REA操作なし。

## 復元できた一次資料

MicrosoftDocs公式repositoryの削除履歴から、archive前の固定commit
`8bcfbd3ed493ba9c386c8be53ff53185307d2cfa` の原文を取得した。
現行Learnページの404やWindows10 WDI mappingをNative仕様の代わりにしない。

- [POWER_MGMT_REQUEST原文](https://github.com/MicrosoftDocs/windows-driver-docs/blob/8bcfbd3ed493ba9c386c8be53ff53185307d2cfa/windows-driver-docs-pr/network/oid-dot11-power-mgmt-request.md)
- [UNREACHABLE_DETECTION_THRESHOLD原文](https://github.com/MicrosoftDocs/windows-driver-docs/blob/8bcfbd3ed493ba9c386c8be53ff53185307d2cfa/windows-driver-docs-pr/network/oid-dot11-unreachable-detection-threshold.md)
- [公式sample N6CSet](https://github.com/microsoft/Windows-driver-samples/blob/d49bb7d72f3651952b2b6e4ad28d14fe2247eb69/network/wlan/WDI/PLATFORM/NDIS6/N6C_OidSet.c): Native setterは4つの離散定数をswitchする。sampleのdefault無条件successは移植しない。

## 省電力24と中間値

Nativeの定義値は0/8/16/24の4つで、任意の連続範囲ではない。
必須PSP機能はFAST(8)とMAX(16)。24はMAXを超えるIHV独自PSPを意味するため、
同型Linux gen3のCAM0/MAX1/FAST2だけから24をMAXへ丸める実装は成立しない。
従って24はNOT_SUPPORTED、1..7/9..15/17..23およびその他未定義値は
INVALID_DATAとして、保持MIBと実profileを変更しない。
0→CAM0、8→FAST2、16→MAX1は既存実CID5/noNeedResp/full HIF提出経路を維持する。
SDK wire定数はcompile時に照合する。これは24を実装したという報告ではなく、
24が必須の未完mappingではないと一次資料に基づき確定したもの。

原文は未接続で非zero PSPの場合、NIC_POWER_STATE MIBを保持したまま実RFを
停止しscan時だけ起動する動作も要求する。現在のCID5提出だけではこの動作を
証明できず、独立したRF-off/scan-only遷移は未実装。source-backedのcombo全停止は
既存NIC_POWER_STATE=FALSE用であり、そのままMIBをFALSEにして代用しない。
この残件をbuild manifestのdeployment blockerへ明記する。PSM全面準拠・省電力の
実測成功と扱わず、firmware ACKを捏造しない。streamingのCAM優先も既存方針を維持。

## 到達不能閾値の実装

OIDはULONGミリ秒で、既定2000。InitializeとMACを含むdefault-MIB resetで戻す。
query/set、supported list、実状態を接続し、非default reset・radio切替では設定を保持。
0は無効化の特別値とせず、IHVのloss条件成立時に待ち時間0として扱う。
ULONG最大まで64bitへ拡張してmicrosecond換算し、乗算overflowや未文書の上限を作らない。

MicrosoftはIHVが到達不能の判定基準を定義することを認めている。この候補では
Linuxと同じ実beacon-timeout event0x13（当該BSS、reason8除外）をloss根拠とし、
検証済みpeer RXからの経過時間が設定値へ達した場合に切断する。
単なるhost無通信で切断しない。eventが閾値より早く来た場合はworkerが保留し、
後続pollで期限を判定する。次の検証済みpeer RXで保留を解除する。
同じRX batchにeventと回復RXがある場合、batch処理後に評価する。

beacon/probe responseはAP送信元/BSSIDとRX metadata、dataは既存security/RX codecを
通った実packetだけを回復根拠にする。OS packet filterやRX pool満杯で通知されない
正常packetもRF受信の根拠となる。送信成功をRX時刻として代用しない。
この閾値はIHV条件内で適用される。firmwareがeventを出す前のbeacon-filter内部や
無線loss検出時間をhostが測定/変更したとは主張しない。workerのpoll間隔や実I/Oの
実行時間による判定遅延もあり、microsecond単位のhard deadline保証ではない。

期限到達時はpacket callbackの外、唯一のPASSIVE SDIO ownerで実session teardown/
cold resetと鍵epoch失効を行ってから、旧peerのUNREACHABLE切断通知を一度送る。
inflight NBLは既存abort/epoch隔離で一度だけ完了し、resetがTX所有を消してしまわない。
teardown失敗はfailure/uncertainを保持し、成功へ変換しない。

鍵OID待機中にもpollから切断が起こりうるため、poll前後のreceive epochと実接続を
検査する。旧鍵transactionを新cold epochへ成功登録したり、新epochを旧transactionの
エラーでpoisonしたりしない。radio/reset/peer-disconnectではloss保留を破棄する。

## 検証とbuild

単一policy fixtureで離散値/未定義値/24、無通信だけでは切断しないこと、
既定/変更/0/ULONG最大の閾値境界、時計逆行guard、回復RXによる解除を確認する。
変更Native TUをWinBlue/ARM32参照headersと1bdのscalar生成flagsでcompileし、
生成assemblyのVFP/NEON命令を検査する。WDK8.1最終link/署名、実接続や切断は未確認。

cleanなこのcommitで既存の `build-mt6630-runtime.ps1` と新しい
`package-mt6630-runtime-1004.ps1` を使用する。出力dirは新規必須。
既存親は従来のcapability/hash/public-interface照合を通して再利用できる。
1003専用package scriptと署名済み1003成果物は変更しない。

```powershell
& .\tools\ci\build-mt6630-runtime.ps1 `
  -OutputDirectory "$PWD\build\native1004-new" `
  -SharedParentDirectory "$PWD\build\existing-parent"
& .\tools\ci\package-mt6630-runtime-1004.ps1 `
  -BuildDirectory "$PWD\build\native1004-new" `
  -OutputDirectory "$PWD\build\native1004-signed-new"
```

署名certの選択は既存helperのCertificateThumbprint引数で明示できる。
統合中の1003をこの作業のために差し替えず、別候補として評価する。
