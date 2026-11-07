# run51：FW-ready後のINIT資源退役1009

## 根因と変更範囲

実配置1008/selected INF1008のrun51、Initialize generation14はoverflowなし。
FW read/hash、WMT/WLAN boot、動的NVRAM read、規制decodeは成功した。
CAL_RUNTIME_BEGINのraw0x53はNTSTATUSではなくA04_WLAN_TIMEOUT(83)。
旧runtime_beginはINITのCPU/TC0 heldをruntime用WTQCR/FFA会計で完全返却するまで待ち、
校正command送信前に失敗していた。Code10/C0000001は上位の結果で、decode失敗ではない。

1009はこの一度限りのINIT境界だけを変更する。1008 SYS/CAT/INFは凍結。
FW opcode、NVRAM record、規制decoder、通常runtime TX返却、OID/cleanupは変更しない。
個体校正値、MAC、private入力の内容/hashを公開source/配布媒体へ含めない。

## 一次source

CT3 vendor `7c4782127d4ea11965be00ce1343056be9bfb769`、
`kernel-4.4/drivers/misc/mediatek/connectivity/wlan/gen3`：

- `common/wlan_lib.c:wlanAdapterStart`：WCIR_WLAN_READYを観測した後WHISRを読み、
  TX_DONEが立っている場合のみHAL_READ_TX_RELEASED_COUNTでINIT分を読み捨てる。
  続いてwlanQueryNicResourceInformationを呼ぶ。
- 同`wlanQueryNicResourceInformation`：FW問い合わせはコメントで、実処理は
  nicTxResetResource。INITの返却がruntime会計で揃うまで待つ処理ではない。
- `nic/nic_tx.c:nicTxInitResetResource`：INIT TC0/TC4は各8buffers。
  `nicTxResetResource`：done/preused/FFAをクリアしてruntime TC予算へ切替。
- `include/nic/nic_tx.h`：runtime TC0=1buffer、TC4=2buffers、13pages/frame。
  既存のTC0=13/CPU=26を維持する。
- `include/nic/hal.h`、`mt6630_reg.h`：WHISR read-clear、TX_DONE bit0、
  WTQCR0..7=0x130..0x14c、WCIR_WLAN_READY bit21。
- 既存XNU `package/wifi/sdio/A04BR3WLANRuntime.h`の
  a04_wlan_runtime_native_boundaryも同じINIT退役を実装している。
  XNU checkoutとLinux source自体は変更しない。

Windows側はより明示的に、全section config ACK、全scatter SDIO完了、Start SDIO完了、
その後のWCIR ready観測によってのみboot_ready_observedを立てる。runtime_beginでは
その証拠・initialized・非runtime・非FW-own・非Native状態・非poisonを要求する。
さらにfresh WCIRのchip ID/readyとWHISR読取りを実施する。異常WHISRは既存WASR取得後
poisonし、release read失敗は具体backend値を保持する。
すべての必要readが成功するまでheld/予算/runtimeフラグを変更しない。
TX_DONE無しの場合に存在しないruntime返却を待たない。これは未完了転送のsuccess扱い
ではなく、FW-readyで完了済みINITとruntimeを区切るvendor規約である。
二度目の呼出しや通常runtimeのoutstandingをこの境界でリセットすることはできない。

## bounded観測

schema1/1624B/64events/最大10flushを維持する。eventを追加しない。
1009以降のStage67 failure detailは以下のreceipt、Stage68 submit detailは
low8=最後の実CID、upper24=同receipt。Stage68の戻り値は実submit結果のまま。
成功時にも実際に完了した境界のreceiptを残すが、NDIS/scan成功とは解釈しない。

|receipt bits|内容|
|---|---|
|0..7|phase:1 gate、2 fresh WCIR、3 WHISR、4 release read、5境界完了|
|8..15|成功したrelease register read本数0..8|
|16|WCIR実観測|
|17|WHISR実観測（異常status含む）|
|18|TX_DONE実観測|

失敗readのregisterはphaseと成功本数から特定できる。
未観測WCIR/WHISRを0やsuccessで補完せず、payload/校正値を記録しない。
`decode-mt6630-checkpoint.py`はbuild1008以前のdetail解釈を保持する。

## 検証・次の導入条件

変更対象の既存hostカード試験で、FW ACK/READY後のTX_DONE有/無の境界、
ready消失、異常WHISR、途中release read失敗、証拠無し/Native状態拒否、
runtime再reset拒否とheld保持を確認した。ASAN/UBSAN実行とC++構文確認。
既存のscan/auth/assoc順序・scanポリシー2件と校正bridge/codec経路も通過。
これはhost検証であり実FWへの校正送信・NDIS開始成功ではない。

固定commitから`tools/ci/build-mt6630-runtime.ps1`、
`tools/ci/package-mt6630-runtime-1009.ps1`でWDK8.1 ARM build/link/SYS署名/CAT再生成。
既存署名親はsource/critical3flags/iface blob/実SYS hashを照合して再利用する。
主担当の次の一括試験で1009配置hash/selected INF/新世代を照合し、Stage68、
以後のSESSION/NDIS attrs、WLAN interface/scan結果を回収する。WiFi単独試験は行わない。
