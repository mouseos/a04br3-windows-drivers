# NDIS 6.30初期化から標準WLAN scanまでの契約

基点 `d751c67`（direct OID修正 `9d3a889` を含む）を一次NDIS sample、SDKの型・size定義、
Microsoft Native 802.11仕様と照合した。この差分はNDIS本体の候補であり、INF1002と
固定37a SYSのrepackage recipeは変更しない。新しいWLAN列挙・scan実機結果ではない。
旧SYSで得たCode31/interfaces0を新修正の失敗として扱わない。

## 明確な欠落として修正した箇所

|段階|旧実装|修正|
|---|---|---|
|adapter registration|Revision1|NDIS6.30のRevision2/対応SIZEOF。NO_HALT/NO_PAUSE等の未実装flagは追加しない|
|general attrs|Revision1、PM capabilities無し|Revision2、必須PowerManagementCapabilitiesExを設定|
|PM capabilities|NULL|Revision2をzero初期化。wake/offload/selective suspendを広告せず、最小wake状態はUnspecified|
|Native attrs|Vista用Revision1|NDIS6.30 SDKで定義されるRevision3/対応SIZEOF、VWiFi/ExtAP/WFDはNULL|
|ExtSTA attrs|Vista用Revision1|Windows8用Revision3、sizeofによるサイズ|
|Native attrsのメモリ|別々のstack変数を参照|Native/PHY/ExtSTA/cipher pair/countryを同一のdriver割当block内へ配置し、登録call後に解放|
|NDIS link|BSS未接続をDisconnected/Halfとして通知|Native 802.11契約のConnected/Full。BSSの成否は既存DOT11接続/切断indicationで報告|
|list setter|一部でHeader.Size/Revision等を未検証|既存対応リストのSDK revision1/type/sizeと件数を検証し、不一致を拒否|
|scanのBSSID|FF wildcardだけを受理|zero wildcardも受理。特定BSSIDを実装したことにはしない|
|scan Request IE|非対応802.11d制御が立つだけで拒否|multi-domainを有効にしない現構成では当該制御を仕様どおり無視|
|scan cache|新scan開始ごとに消去|既存BSS cacheを保持し、明示FLUSH_BSS_LISTで消去|

PM能力構造の提供はsleep/resumeの実装成功ではない。NativeのConnectedはNDIS mediaの意味で、
APへの接続、認証、鍵導入、port authorizationや通信成功を意味しない。
新規MMIO、firmware command、DMA、親interface、NVRAM、Bluetoothはこの修正では変更しない。

## 登録順とaccepted versions

`DriverEntry` は6/30、driver characteristics Revision2を登録し、実登録statusをそのまま返す。
initialize/halt/unload/pause/restart/OID/cancel/send/return/cancel-send/PnP/shutdownと
9d3a889のdirect OID/cancel-direct OIDのpairが設定されている。
CheckForHangを登録しないため、対応するReset callbackを推測で追加しない。
SetOptions、Wi-Fi Direct、offload、RSS等の機能も登録したことにしない。

`RuntimeInitialize` はregistration→親照会/実hardware start→general→Nativeの順。
属性登録失敗はそのNDIS statusを返し、hardware start失敗も成功へ変換しない。
PHYは既存ERP単一PHY/Revision1。STA専用でAP/Virtual WiFi/WFD capabilityは広告しない。
list setterはSDK定義のSSID/BSSID/auth/cipher/PHY ID/privacyリストのRevision1を受理する。
SCAN_REQUESTはHeader付き構造ではなくDOT11_SCAN_REQUEST_V2。

MicrosoftのNative登録手順は、MEDIA_SUPPORTED queryでNative802_11を確認し、
CURRENT_OPERATION_MODEを設定、RESET_REQUEST methodを成功させてから標準WLANへ登録する。
既存codeはこれらの入口を持つが、実cold-start/RESET成功をまだ観測していない。
INF staging、署名、force bindの成功だけではこの手順の成功を証明しない。

## 必須OID・容量の確認で残った実装差

以下を架空の値や無条件successで埋めない。本候補だけでNative 802.11全面準拠とはしない。

|仕様で要求される機能|現在のソースの差|今回の扱い|
|---|---|---|
|TX/RX queue各64以上|Native attrsは各1、実TXにも単一/次要求の制限|数値だけ64へ増やさない。実queue/所有/rundownが必要|
|scan SSID list最低4|attrs/codecは1まで|複数SSIDを黙って捨てない。実wire command/結果統合が必要|
|excluded MAC list最低4|attrsは0、OIDも未実装|実保持と候補除外を伴う実装が必要|
|Native MTU2304|現在general MTU1500、TX buffers/parserも小さい|MTUだけ2304へ上げない。実転送上限の整合が必要|
|ExtSTA required OIDs|ACTIVE_PHY_LIST、ASSOCIATION_PARAMS、ENUM_ASSOCIATION_INFO、EXCLUDED_MAC_ADDRESS_LIST、HIDDEN_NETWORK_ENABLED、MEDIA_STREAMING_ENABLED、POWER_MGMT_REQUEST、SAFE_MODE_ENABLED、STATISTICS、UNREACHABLE_DETECTION_THRESHOLD等がsupported list/dispatchに無い|正しい状態/実統計/動作を持たないstub successは追加しない|
|Operational required OID|DOT11_MULTICAST_LISTが無い。NIC_POWER_STATEのoffも未実装|設定後の受信filterやradio停止が必要|
|general mandatory統計等|GEN_STATISTICS/XMIT/RCV counters等の入口無し|実成功byte/packet/error accountingが必要。ゼロ捏造をしない|
|標準scanの全形態|特定BSSID、restricted、明示PHYリストは拒否。autoは現codecではpassive。接続中scanも拒否|未対応を明示し、対応済みと広告しない|
|Native packet filter|data directed/broadcast/multicastのみ、management frame indication等は未実装|filter bitだけ増やさない|

必須表にはIBSS_PARAMSもあるがWindows8.1ではIBSS自体がdeprecatedで、現codeはIBSSを
広告しない。この項目をSTA専用実装への無条件stub追加の根拠にしない。
安全な単純値の修正と、実機能・並行性・firmware契約を必要とする残件を分けている。

scanの受付OIDは既存PASSIVE workerから実command受付後に成功完了し、SCAN_CONFIRMは
実matching event後だけ送る。イベント不明・timeoutをscan成功にしない。
BSS methodは既存可変長構造で返す。完全な標準scanの受理やWlanEnumInterfaces成功は未確認。

## 一次根拠と確認範囲

- [Microsoft netvmini adapter sample（固定source）](https://github.com/microsoft/Windows-driver-samples/blob/d49bb7d72f3651952b2b6e4ad28d14fe2247eb69/network/ndis/netvmini/6x/adapter.c)：version別registration/general/PM構造の設定。仮想Ethernet sampleのためNative固有設定の根拠には使わない。
- [NDIS6.30実装要件](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/implementing-an-ndis-6-30-driver)、[6.30構造](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/using-ndis-6-30-data-structures)。
- [General attrs](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/miniportgeneralattributes/ns-miniportgeneralattributes-ndis_miniport_adapter_general_attributes)、[PM capabilities](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddndis/ns-ntddndis-_ndis_pm_capabilities)。
- [Native attrs配置/最低queue](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wifiattributes/ns-wifiattributes-ndis_miniport_adapter_native_802_11_attributes)、[ExtSTA revision/最低容量](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-dot11_extsta_attributes)。
- [Native MiniportInitializeEx](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/miniportinitializeex-requirements)、[Native登録手順](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/native-802-11-miniport-driver-registration)。
- [ExtSTA必須OID表](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/native-802-11-extensible-station-oids)、[Operational必須OID表](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/native-802-11-operational-oids)。
- [Explicit scan受付](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/requesting-explicit-scan-operations)、[cache保持/完了](https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/performing-explicit-scan-operations)。

SDK上のrevision/size定義をARM32 compileで照合し、既存scan parserと実event bridgeの
限定host回帰でwildcard/境界/不一致event/失敗を確認する。新しい実機実験や大量模擬試験は追加しない。
参照SDK headerによるcompileはWDK8.1 SYS最終link・実OS受理の代替ではない。
このNDIS sourceは固定1002/77b53ac0候補とは別物なので、それへ同じ名前で上書き混入しない。

## 後続1003候補

上記は68a6f16時点の確認記録。実queue/MTU/SSID/excluded/統計の後続実装、
一次根拠、残る阻害とbuild/sign手順は [NATIVE-1003.md](NATIVE-1003.md) を参照。

## 後続1004の残件更新

027f31d以後の未接続PSP・実BSS停止/LP ownership・scan復帰の実装と現在の
source/実機残件は [NATIVE-1004.md](NATIVE-1004.md) の「現在の残件一覧」を参照。
68a6f16当時の上表を現行実装の未反映項目として再試験しない。

## NDIS起動前のINF欠落（1005）

1003/1004のDDInstall必須keyword配置に未反映の欠落を確認し、INF1005で修正した。
原ProblemStatusの正しい定義、一次根拠、既存署名1004 SYSを保持するrepackageと確認条件は
[INF-1005.md](INF-1005.md) を参照。C側属性だけでNDIS起動前の必須INF設定を代替しない。
