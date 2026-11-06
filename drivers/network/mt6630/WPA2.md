# Native 802.11 WPA2-PSK / AES

WindowsのNative Wi-Fi supplicantがPSK/PMK/PTK導出、EAPOLのMIC/replay検証、
GTK unwrapを担当する。driverはパスワードを受け取らず、独自supplicantの検証を迂回しない。
OSの鍵OIDから16-byte CCMP TKとRSCを受け、MT6630 gen3のCID7へ渡す。

- OID_DOT11_CIPHER_KEY_MAPPING_KEY → pairwise CMD_802_11_KEY。
  実EVENT_ID_ADD_PKEY_DONE (0x24)を待ち、相手/BSSを照合する。
- OID_DOT11_CIPHER_DEFAULT_KEY → key ID別group WTBLとCID7。
  Linux同様noNeedRespのためHIF送信完了まで。firmware適用ACKとは呼ばない。
- OID_DOT11_PORT_STATE_NOTIFICATION → controlled port。関連付け完了だけでは開かない。
- EAPOLの暗号化はOS send contextのexemption指示と鍵状態に従う。
  association通知前の早着フレームを保持し、通知後にOSへ渡す。
- 同一鍵の重複設定は再送・PNリセットしない。削除済み同一鍵の同epoch復活を拒否。
  GTKはkey ID別に保持し、rekey中の重なりに対応する。
- CCMP RXはHWエラー、WTBL、key ID、PN、privacy exemption、portを確認する。
  作業TK、RX/TX scratchは使用後に消去する。鍵・SSID・PSKのログ出力は追加しない。

対応はRSNA-PSK/CCMP、ERP20の最小構成。APのMFPCは許容するがPMFは選ばず、
MFPRは拒否する。TKIP/WPA3/必須PMF/QoS/ローミングは対象外。
RXはHW検証後にIV/MICを除去した24-byte header + LLC/SNAP形式だけを受理する。
このHW RX形式は実機未確認。未知の配置を推測して復号済み扱いにはしない。

同型番Linux正本 a04br3-linux-drivers
`3d2c74e252d6f34a5980deda96bdc04064108d74` の
wifi/mt6630-gen3/common/wlan_oid.c (wlanoidSetAddKey/RemoveKey)、
include/nic_cmd_event.h (64-byte CMD_802_11_KEY)、nic/nic_rx.c、
include/nic/nic_rx.h を参照した。include/wlan_oid.hとnic/nic.cの直接代入から
BSS wire値をOPEN=0、WPA2-PSK=7、CCMP encryption status=6へ修正した。

ARM32 COFF compileの確認と実接続は別である。最新WDK8.1 SYS link、OSの属性/OID受理、
EAPOL往復、鍵event、暗号化データ送受信、DHCPは主担当の実機確認が残る。

接続候補の選択では同じSSIDの先頭BSSだけを検査せず、候補ごとにERP対象channel、
NVRAM由来permitted/active mask、OPEN/RSNA-PSKとCCMP RSN policyを確認する。
先頭がOPEN、非対応group cipher、使用不可channelでも、後続の適合BSSを選べる。
Native association通知とfirmware station選択は同じpredicateを使う。
同型番Linux gen3 mgmt/scan.cのscanSanityCheckBssDescもchannel legalityと
rsnPerformPolicySelectionを候補ごとに評価する。新しいSSID/BSSID/鍵のログは追加しない。
既存bridge確認へ混在候補の回帰確認を1件追加し、模擬NDISや試験群は追加しない。
この変更で実接続を確認したとは扱わない。接続中scanは引き続き明示拒否される。
