# scan後の標準接続境界 Native1013

scan成功確認済みのNative1012（b21b8693）は凍結し、別branchで接続開始までの未接続箇所を修正する。実機・保存ネットワーク・資格情報は扱わない。1013の接続、認証、暗号化通信、DHCPは実機未確認。

## 実装した2箇所

1. Native scanは`wmt_bridge.scan`に実firmware SCAN_DONEを記録し、`a04_native_scan_indicate`が完了通知時にcompleteを消費する。一方`a04_wlan_station_select`は別の`station.scan.complete`を必須とし、1012のruntimeはそこへ実完了を渡していなかった。実runtimeのpacket dispatcherで、matching SCAN_DONEの検証成功時にだけstationへscan記録を渡す。connect時に成功flagを作らず、元の未完了/sequence不一致拒否を維持する。BSS cacheを保持している既存Native経路と、XNU由来station codecの前提を接続する。
2. `native_control.connect`はSSID/BSSIDの最初の候補を選んでから暗号条件を調べていた。同一SSIDで不適合な暗号方式・channelのAPが先頭にあると、後続の適合候補へ到達しない。Linux gen3 `scanSearchBssDescByPolicy` / `rsnPerformPolicySelection`と同様に、候補ごとに許可channelと現在のOS選択（OPEN/NONEまたはRSNA-PSK/CCMP）を検査する。既存のexcluded/desired/association BSSID制約を維持し、PMF必須・非CCMP・不正RSNを許容しない。

## 確認した既存経路（重複実装しない）

|境界|既存の接続先|条件・未確認事項|
|---|---|---|
|標準WLAN接続|OID_DOT11_CONNECT_REQUEST → native_control.connect → station_runtime_connect|今回2箇所の修正。接続完了を合成しない|
|802.11認証・関連付け|実channel grant → Open auth → Association response → CMD_UPDATE_STA_RECORD → matching ACTIVATE_STA_REC|legacy ERP20。FW response/MAC/BSS/sequenceを検証|
|OS認証|Native association/connection indications → OS supplicant|PSK/PMK導出、EAPOL MIC/replay、GTK unwrapはOS担当。資格情報をdriverへ収集しない|
|鍵設定・port|標準pairwise/default key OID → gen3 CID7、PTK実ADD_PKEY_DONE → PORT_STATE_NOTIFICATION|既に実装。association成功だけでportを開かない|
|EAPOL送信|NDIS NBL → OS exemption context → Native conversion → TC4送信 → matching TX_DONE|既存OS管理経路を使用。独自supplicantを追加しない|
|データ送信|NDIS NBL → Ethernet conversion → OPENまたはOS CCMP/TX descriptor → SDIO → matching TX_DONE|ページcreditとTX完了を区別。未観測ACK/成功を作らない|
|データ受信|実HIF descriptor/group3、CCMP時group1 PN/WTBL/key検査 → owned RX slot → NDIS indicate|既存filter/port/replay拒否を維持。IV/MIC除去済みraw24byte header形式の実機確認が残る|

既存の制限はRSNA-PSK/CCMPとOPEN、2.4GHz ERP20。TKIP/WPA3/必須PMF/QoS/roamingをこの変更で実装済み扱いしない。scan成功も接続・通信成功とは区別する。

## 根拠と検証

- Linux gen3: `mgmt/scan.c`の`scanSearchBssDescByPolicy`（暗号不適合候補では次候補へ）、`mgmt/ais_fsm.c`の候補選択→channel/auth遷移、`mgmt/cnm_mem.c`の実STA activation応答。
- Linux gen3: `common/wlan_oid.c`のCID7鍵形式、`include/nic_cmd_event.h`のkey/activationイベント、`nic/nic_rx.c`、`nic/nic_tx.c`。既存鍵・TX/RX実装の再実装は不要と判断。
- XNU由来の既存`vendor_xnu/A04BR3WLANStation.h`は実scan完了後だけstation_selectを許可する。この前提をNative runtimeにも接続する。
- 標準API: https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-connect-request / https://learn.microsoft.com/en-us/windows-hardware/drivers/network/native-802-11-send-operations 。WDK8.1 Native 802.11契約を維持。
- 小規模な結合fixtureで実runtime dispatcher→bridge SCAN_DONE→Native scan confirm→実station selectorのBSS/STA/channel要求まで確認。旧1012 runtimeでは同じfixtureがstation.scan未反映で停止することを再現。RF auth成功は合成しない。
- 同一SSID候補の暗号/channel検査、既存のauth/association/STA activation順序、NBL完了とHIF RX decodeを限定的に検証。大量の形式試験を追加しない。

増分buildは1012の固定manifest/unsigned SYSを検証し、変更したcheckpoint/native_control/station_runtimeの3objだけcompileする。rsn_policyはこの2callerだけがincludeし、構造体・公開親interfaceは不変。他objはcopy後hash照合し、変更pathのallowlistを検査する。新SYS/CATと公開certを回収し、実機への投入は主担当に委ねる。
