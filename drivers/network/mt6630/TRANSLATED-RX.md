# MT6630 translated HIF受信のNative 802.11接続

## 基点と不足

基点は実provider1027 source `73427b486f3e621dd998774f38f396fbac54a61d`。
保存fresh200の `mt6630-combo-runtime.sys` と既署名provider1027は同一SHA256
`97dcede955d813a7d72ba8d6e950ceae1215c94351187cf18c73cc15d3f47334`。
既署名manifest SHA256は `fbe1a4dc4101726d21120212ce371740421d1491c3d8a94e33369aec9a3277a2`。
この既署名証明は今回変更後のbuild/sign証明ではない。

1027は接続手順、OS supplicant鍵OID、CID7/PTK完了、controlled port、Native NBL送信、
実TX_DONEとpage creditの分離を既に持つ。一方でWindowsのデータ受信decoderは
`RX_STATUS_HEADER_TRAN` を常に拒否していた。
既存 `vendor_xnu/A04BR3WLANData.h:a04_wlan_ethernet_rx` はtranslated Ethernetを扱うが、
WindowsのOS鍵所有者へそのまま接続できる実装ではない。
station_setupのBASIC_CONFIGは `ucNative80211=0`、送信もEthernet HIFを使う。
translated RXを受け取った場合の欠落はsource上で確認できる。
**実機で当該形式の受信が観測された、またはこれが実機通信失敗の唯一原因と確認した、とは主張しない。**

根拠はボードvendor `CT3_GPL_LGPL_Src/kernel-4.4/drivers/misc/mediatek/connectivity/wlan/gen3/`：

- `include/nic/nic_rx.h:123` header length/offset/translation、`:477` group4のFC/TA/SeqFrag/QoS/HTC。
- `nic/nic_rx.c:687` translated時はgroup4からFC/sequenceを取得して重複を判定する。
- `common/wlan_lib.c:3168` BASIC_CONFIGの `ucNative80211=0`。
- 既存XNU移植codec `vendor_xnu/A04BR3WLANData.h:16` translated Ethernetとdescriptor群の配置。

## 変更した経路

`native_hif_rx.c:a04_native_hif_rx_decode_owned` を追加。
raw受信は従来decoderへ渡し、translated時だけ非重複のadapter-owned `rx_bytes` へ
Native MPDUを復元する。group4/3を必須とし、CCMPはgroup1も必須。
DA/SAと実TAからFromDSのアドレス、実group4からFC/sequence/QoS/HTC、
実group3からPHY/rate/RCPIを使用する。未保存のdurationは0で、air captureの原本とは扱わない。
EtherType形式だけRFC1042/SNAPへ戻し、802.3 length形式およびbridge-tunnel例外は拒否する。
24/26/30byte MAC、group2有無、offset0/2、broadcast/multicastを境界内で扱う。

`ndis_runtime.c:receive_data` のOPEN、association通知前の一時保持、
`native_security.c:a04_os_security_receive` のWPA2を同じdecoderへ接続する。
既存の鍵slot/WTBL/PN/OS privacy exemption/controlled port、association世代のretry判定、
packet filter、Pause/stopとqueue上限を通してから最終owned slotへcopyする。
鍵導入・port open・受信成功は合成しない。fragment、A-MSDU/AMPDU、error、
未対応cipher、TA不一致、必要group欠落、未知radio metadataは引き続き拒否する。

BT broker/SCO、共有親、GPU、音声、電源停止証明、firmware、INFは変更していない。
このbranchはsource候補であり、新版package/versionまたは媒体を生成していない。
旧1027 manifestのsource/署名/実機verified値を今回候補へ流用しない。

## 限定host検証と引渡し

```
make test-mt6630-translated-rx
```

既存raw RX/NBL/receive/scan-connect/async Pauseも確認する。
新テストはARP/IPv4/IPv6/EAPOLの変換、group/長さ/overlap/不正FCを検査し、
実 `a04_os_security_receive` 関数を抽出して実decoderと組み合わせ、
PTK/GTK、WTBL、PN、閉じたport、ICV、APのみのEAPOL exemptionの拒否条件を確認する。
これはWindows NDIS実行やfirmware応答・通信の証明ではない。

Windowsビルド・署名・実機適用はmain所有。SDK/driver全体の再buildは要求しない。
既1027 manifestのsource_closureでは変更header依存を含め、
`native_control`、`native_contract`、`native_security`、`native_receive`、`native_hif_rx`、
`ndis_runtime`、`combo_broker` の7TUが再compile対象となる。
後者brokerはheader依存による対象で、broker実装/ABIの変更はない。
残るobjは最終source closureとcompile flagsの完全一致を確認したものだけreuseする。
新source/versionを明示したmanifestと新SYS/CAT署名検証はmain側で別途必要。

既観測記録はscan1012成功と1020接続/QoS HANDOFFを参照した。
provider1027登録やscanを接続・認証・DHCP/IP取得・実通信成功へ読み替えない。
今回、VM/runner/実機操作は0。次のbaselineはmainが確定するfresh状態を使い、
GPU更新後へfresh200/198のnative proofを改名・流用しない。
公式PowerShell修正版 `5c39277db113faa98cd3a0f26c1072a7ce3d76c2` は変更しない。
