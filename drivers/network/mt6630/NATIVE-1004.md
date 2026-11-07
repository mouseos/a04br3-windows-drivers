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
休止しscan時だけ起動する動作も要求する。027f31dではCID5提出だけだったため
未実装のblockerとして残した。後続の `work/mt6630-native-power-1004` は、下記の
Linux AIS deactivate/LP ownership経路を実装してこのsource欠落を解消する。
PSM全面準拠・省電力の実測成功とは扱わず、firmware ACKを捏造しない。
streamingのCAM優先は接続中の既存方針を維持する。未接続では非zero要求に従い休止する。

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

## 未接続PSPの実RF-idle制御

Linux正本 [CT3_GPL_LGPL_Src](https://github.com/s1204IT/CT3_GPL_LGPL_Src) の固定
`7c4782127d4ea11965be00ce1343056be9bfb769`（以下の関連filesはclean）を照合した。

|元source (kernel-4.4/drivers/misc/mediatek/connectivity/wlan/gen3/)|根拠|
|---|---|
|mgmt/ais_fsm.c AIS_STATE_IDLE 870行以降|未接続でネットワークを非activeにしnicDeactivateNetwork。SCAN/LOOKING_FOR 1154行以降はnicActivateNetwork|
|nic/nic.c nicActivateNetwork/nicDeactivateNetwork 1412/1473行以降|既存CID0x11/12byte payload。deactivateは全zeroからBSS/OwnMACだけ設定。activeにはMAC/BMC indexを設定|
|nic/nic_pwr_mgt.c nicpmSetFWOwn 83行以降|保留IST処理後にFW所有を要求し、WHLPCR readbackで失敗を判別。拒否時はdriver ownへ戻す|
|nic/nic_pwr_mgt.c nicpmSetDriverOwn 148行以降|driver ownを要求して実bitを待つ。1ms間隔、総2048ms、256iterationごと再要求|
|include/nic/mt6630_reg.h WHLPCR 264行以降|bit8 write=FW_OWN_REQ_SET/read=IS_DRIVER_OWN、bit9=FW_OWN_REQ_CLR|
|include/config.h CFG_ENABLE_FULL_PM=1、include/pwr_mgt.h|LP機能が有効。唯一のownerのpower-control参照が0でFW ownershipへ戻す|
|os/linux/gl_kal.c HIF thread 2500行以降|CMD/TX/ISTを処理してからpower-controlをreleaseする|

このdriverの既存bootstrapが採用するCHLPCR workaroundを維持し、ownershipの書込みは
CMD52/function1/byte1(address5)、値1(FW要求)/2(driver要求)だけに限定する。
CMD53書込みやIRQをenableする低byte変更は追加しない。新FW opcode、任意MMIO、
新DMA、WMT全combo停止命令は追加しない。

非接続・非zero PSP・scan/connect非実行中だけ、実CID0x11でAISをdeactivateする。
唯一のPASSIVE ownerで実RX/ISTとWTQCR/FFAをdrainし、全管理TCの実used pagesが0かつ
空WHISRを観測してからFW所有を要求する。FW ownershipのreadbackが失敗したら
休止成功へ変換せずfailureとする。credit未返却や割込みが100回/1msでdrainできない
場合もfailure。数値をリセットして待ちを飛ばさない。

これはLinuxと同じBSS停止/LP経路によるRF-idle要求とownership確認であり、アンテナ出力や
消費電流の物理測定、chip全電源OFFの確認ではない。NIC_POWER_STATEをFALSEにしたり
PHY_STATE_CHANGEDを送ったりせず、combo/host/RTC leaseを保持する。これにより他clientへ
使用中資源を解放せず、通常の休止/復帰でfirmware再load・BSS cache消去もしない。

休止中は保留RX/切断処理がなければ定期DPCからwork itemを割り当てず、
定期workerもHIF statusを読まない。HIF read/write/send/receiveにもFW所有中の
通常アクセス拒否guardを置く。設定query、cache取得は休止状態のまま処理する。
scan/connect前には実driver ownershipを取得し、BSSをactivateしてから要求を提出する。
scanは入力検証後にだけ起こし、実scan-done event後に再休止してからSCAN_CONFIRMを
成功通知する。再休止失敗をscan成功としない。接続成功後はBSSを維持して既存PS profileを
使う。CAM(0)への変更も実wake/activateを伴う。

peer切断はpacket callback内でradio stateを破壊せず、poll後のownerで既存cold resetを
行い、保持PSP要求に応じて未接続休止へ戻す。timeout/cancel等でscanが失敗した場合は
失敗通知を維持し、必要な後処理には既存resetを一度だけ使う。未知のscan-abort opcodeや
無限retry/reconnectは追加しない。既存cold reset時のcache初期化は通常のidle cycleとは
区別する。default-MIB resetはpolicy更新が終わるまでidleへの移行を保留し、古いPSP要求で
一旦寝てからCAMへ戻す無用な遷移を避ける。明示radio OFF/Haltは既存実quiesceを使う。

## 現在の残件一覧

|項目|source状態|実環境で必要な確認|
|---|---|---|
|未接続PSPのBSS停止/LP移行/scan復帰|実CID/実credit drain/LP readbackとruntime接続を実装済み|WDK最終link/署名、未接続/scan反復/接続/切断時の電力・動作|
|1004 unreachable閾値|実loss根拠・可変ms・default reset・epoch検査を実装済み|実loss/回復/閾値変更と一回だけの切断|
|0/8/16 PSP|source-backed CID5とidle/awake遷移あり|接続中PM動作と通信、未接続RF-idleの実測|
|24/中間値|仕様確定。24は未提供IHV拡張、中間値は未定義として拒否|必須機能の欠落を数値mappingで装わない|
|scan形態|4SSIDと通常active/passive/auto。特定BSSID/restricted/明示PHY/接続中scanは非対応のまま|全面ExtSTA対応を主張しない|
|power機能|自律auto-PS/WoL/offload/modern standby/sleep-resumeは広告しない|別scope。今回のLP休止をOS sleep対応に代用しない|

既存policy fixtureを拡張し、実codec/HIF helperのdeactivate payload、BSS activate payload、
未返却creditでFW-own禁止、readback拒否、休止中HIF拒否、復帰ACK、2048ms timeoutを確認した。
新しい模擬試験群は増やしていない。関連11件のhost回帰と全40TUのARM32/scalar compile、
生成assemblyのVFP/NEON0命令を確認した。これはWDK8.1 linkや実機のRF停止実測の代替ではない。

INF1004、build/sign手順、既存親再利用条件は前掲のまま。署名済みruntime1003/1bd250cと
旧1003 package scriptは保持する。build manifestは実idle経路と未測定状態を記録し、
deployment_ready=falseを維持する。
