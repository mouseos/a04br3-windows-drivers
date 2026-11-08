# MT6630接続・QoS統合候補1020

scan成功1012を共通祖先として、接続修正1013 `8fd9b62d5af47691a40b2726311b5415ff46aa4a` と
QoS修正1014 `6e6bd36b4b05d345b1b468a7cf988ea93e55e1bd` をmergeした候補。
旧1012/1013/1014の署名成果物は変更しない。実機接続・通信・性能は未確認。

## 統合判断

機能コードの競合はない。Makefileの両方のhost境界targetを残し、checkpointとINFを1020へ揃える。
1013の実SCAN_DONE保存と候補別security/channel検査を取り込み、1014のheader/TID/owned buffer処理を保持する。

本機Linux gen3 `mgmt/scan.c` の `scanSearchBssDescByPolicy` はSSID/BSSID一致候補でも
`rsnPerformPolicySelection` に失敗したら次候補へ進む（2527付近）。`mgmt/rsn.c` はprivacyと
OSの暗号要求を照合する。1013と同様にOSのOPENまたはRSNA-PSK/CCMP要求、BSSID/excluded制約、
active channelを満たす候補を選ぶ。任意の先頭BSSへfallbackしない。

stationの接続開始に使うscan.completeはmatching firmware SCAN_DONEをbridgeで検証した後だけ保存する。
Native scan confirmがbridge側記録を消費してもstation側の観測記録を残す。connect時に成功を合成しない。
QoSが接続選択や鍵/controlled port検査を緩和することもない。

Native802.11受信優先度、CCMP replay index、TXD TIDの根拠と対応範囲はQOS-1014.mdを参照。
WMM/全AC/reorder/roaming/TKIP/WPA3/必須PMFは新規対応していない。

## 検証と実機の判定条件

hostは `make test-mt6630-scan-connect test-mt6630-checkpoint` とQOS-1014.mdの3targetで検証。
実runtime dispatcher→matching SCAN_DONE→Native confirm→station selectorの接続要求まで、
QoS可変長/TID/最大長/入力不変、NBL寿命/実TX_DONE/credit、checkpoint1020を確認する。
fixtureの接続要求到達を実AP接続成功と呼ばない。

main後段の実機試験では、選択INF/SYS hashとcheckpoint1020を最初に確認する。
標準WLANのscan complete/結果取得、association notification、OS認証、controlled port、IP設定、
実データ送受信をそれぞれ別に判定する。成功通知だけでデータ通信成功としない。
既存の利用可能なOS profileがある場合だけ標準WLAN APIを使い、profile名・SSID/BSSID・鍵は
ログや公開物に書かない。存在しなければ接続条件未成立とし、新SSID/credentialsを要求しない。
鍵materialを含むOID payloadや保存profile XMLを診断目的でdumpしない。

新規媒体や端末操作はこの作業に含めない。親/GPU/nativeUMD/hive/他owner sourceを変更しない。
`tools/ci/rebuild-mt6630-connect-qos1020.ps1` で固定1014cacheから3objをcompile、38objを再利用する。
