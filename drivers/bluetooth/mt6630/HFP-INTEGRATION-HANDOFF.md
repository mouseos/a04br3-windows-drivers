# MT6630 BT / HFP の統合依存

## 担当境界

BT専用sourceとtoolsのみ変更した。Raman管理のAudio/親、凍結USB1004/SD3/親1010、
版予約1009/1012/1014は変更しない。親の新versionは未予約であり新親SYSを生成しない。
Raman source参照は `4b2a461245274728d01c86e2dad833b0fa398b5f`。
provider v2のstructは `bf98963ea20fc94c8b42b3c05f1863af1bf92b08` と同一。
追加関数 `a04_bt_hfp_request_connection(session, BOOLEAN)` はBT-privateのみ。

Ramanの `build-hfp-portcls.ps1` / `build-hfp-owner-modules.ps1` のBT pinは
旧63c32のまま。統合時は本branchの固定commitへ全5TUとheadersを一括pinする。
`hfp_volume.h` も含め、旧coreと新coreを同時linkしない。
今回raw SYSはendpoint coreを含まないため、Audio側coreと重複しない。
両topologyにoneshot SLC connect/disconnectを実装したが、raw HCI起動や
request受付だけをWindows標準radio列挙・SLC・音声成功と判定しない。

## 標準stack接続を妨げる未確定契約（mainへの調査依頼）

1. `transport.c:a04_bt_initialize` の GORM bytes8..11 → vendor command FC72。
   元Linuxは4bytesをそのまま送るだけでmaster/slave、rate、frame/word幅、極性の
   定義がない。値を推測せず、vendorのcommand構築・PCM設定分岐をREAで追う。
   確定後 `A04_HFP_CONTROLLER.ValidatePcm` に実設定とCVSD8k/mono/S16整合を接続する。
2. `A04_HFP_CONTROLLER.MatchHfpTarget` の物理radio照合。
   `IoGetDeviceObjectPointer`で得たHFP targetとBthMini/HCI PDOの実関係が必要。
   WDK8.1 `IOCTL_BTHHFP_GET_DEVICEOBJECT` は宣言のみ確認できるが、出力型、
   参照所有権、返却objectがHFP/BthEnum/physical radioのどれかが未確定。
   mainにWinBlue bthhfenum/bthhfaud/bthportの該当dispatchのREA調査を依頼する。
   GUID/名前一致を物理一致に代用しない。未知IOCTLは発行しない。
3. `A04_SHARED_HFP_INTERFACE.VerifyDmaQuiescent` はRaman実装でNULL。
   `hfp_memif` のDL1/I05→O02再生、DAI/O11録音について、独立した方向別bus-drainの
   保証が必要。CUR停止・disable readback・IRQ flushだけを証明扱いしない。
4. `A04_HFP_CONTROLLER.CodecLatency` の実codec delay、Audio FIFO delayも未確定。
   0を成功値にせず、HCIBypass None/0を維持する。

Linux根拠は既存 `PROVIDER-EVIDENCE.md` / `SIDEBAND.md`。
`CT3_GPL_LGPL_Src/kernel-4.4` の固定source
`7c4782127d4ea11965be00ce1343056be9bfb769`、
`port/edk2/a04br3-linux-drivers` の固定source
`3d2c74e252d6f34a5980deda96bdc04064108d74` を参照する。

REA用公開artifactはvendor source `0c756f413e7011231c8fb793af0930311023f68d` の
`proprietary/vendor/lib/`。root内の調査用コピーは
`port/gsi/logs/ios-feasibility/windows-rt-mt6630-hfp-resume-20261107/vendor-reference/`。
端末固有recordを含めず、成果物へvendor SOを同梱しない。

| Artifact | SHA256 |
| --- | --- |
| libbluetooth_mtk.so | 77e897889ea40a09ced2baf0194b92edc97720c963c7a484a8529962e7bea429 |
| libbluetooth_mtk_pure.so | 8685b6734c639dfccbf63ba611154389961848d11dc53daec724bb5c60c0fd2d |
| libbt-vendor.so | a7f356a7d5ba7399c0155b5cffc385b3582c957852059b0ece8608db0b078495 |

## 排他と所有権

WiFi v2親leaseはMSDC2/電源/FWの排他combo ownerとして継続使用する。
WLAN PDOなしのBT profileであり、同時WiFi成功は広告しない。
Audio用HFP leaseはこのcombo leaseとは別で、実GPIO/AFE/DMA ownerが必要。
共存には単一combo firmware/status reader/電源ownerへの設計変更が必要であり、
今回別ownerを追加しない。MAC/校正値は既存read-only NVRAM経路で動的取得する。

## 再現build

固定commit・clean checkout・Windows WDK8.1 ARM32/KMDF1.13・LLVM clang/lld-linkを使用。

```powershell
& tools/ci/build-mt6630-bluetooth.ps1 -OutputDirectory "$PWD/build/bt-raw"
& tools/bluetooth/build-mt6630-hfp-endpoint-core.ps1 -OutputDirectory "$PWD/build/bt-hfp-core"
```

前者はraw SYS/INF2003を生成。親は再buildせず、別途既署名の排他BT matched親が必要。
後者は5TUのlibrary/manifestを生成する。HFP core単独をSYS/CATや実音声成功として扱わない。
実Audio providerの障害条件が解決するまでHFP統合親の署名・実機試験を要求しない。
署名は正規WDK8.1 Inf2Cat `/os:6_3_ARM`、固定証明書でSYS/CATへSHA256署名し、
CAT membershipとsource/hash manifestを確認する。実機はmainのみ。


## Ramanとのsource非変更の組合せ確認

`tools/bluetooth/build-mt6630-hfp-paired-core.ps1 -OutputDirectory "$PWD/build/hfp-paired"`
はRaman固定 `4b2a461` のAudio/親headersをarchiveし、Audio sourceを一切編集せずに
本BT headersでowner3TUをcompileする。本BT5TUと合わせて明示8objectsだけを
単一libraryへarchiveする。旧63c32のobjectを混ぜない。依存全file hashと両source commitを
paired manifestへ記録する。親SYS/INF・版予約・flagsは変更しない。
これはAudio commonとBT coreのWDK組合せ確認であり、既存Audio backendとの最終SYS link、
controller登録、DMA drain、HCIBypass資格や実音声を証明するものではない。

raw署名候補は `2452d9885654a13138d01d5c5cd5dee54e99f351` のSYS/INF2003、
HFP coreは `28529e63e176076029b10e7c94fd65a2981a1259` の実WDK8.1全5TU archive。
raw source/flags/hash一致を確認し、同一SYSを再利用した。両者の出自は署名manifestに分けた。
回収ZIP SHA256: `860c4407189fa610353b5a08e1a4a8e1f9253da044ad93cec017ddf6a8d7f6e2`。
このZIPに親は同梱しない。旧02e raw候補の自動置換・実機試験は要求しない。

## 今回のRaman宛結合要求: transport登録v2

`hfp_controller_contract.h` はRamanのv1と**同じfield順/size**の提案v2。
BT startupはreal WMT/FC72/ReadLocalVersion後に同GUID/version2だけをqueryし、
実Reference/Dereference/Acquire/Live/Release/MatchHfpTargetを登録する。
Acquire/Releaseは本物のKMDF D0 holdと現combo generationを保持する。
BT Stopは未解放holdや親Unregister失敗で電源/leaseを解放しない。
radio surprise removalはLiveを失効し、Audio DMA drainの証明に代用しない。

FC72の実PCM解釈とcodec latencyは未確定のため、ValidatePcm/CodecLatencyはNULL。
常時成功/常時拒否callbackを足さない。現在の親v1へv2descriptorを渡さず、query非対応は
raw HCIのみ継続する。v2登録成功もHFP factory/標準stack資格を意味しない。

Ramanへ必要な変更（BT側では共有sourceを編集しない）:

1. 親query version2を同layoutで追加し、RegisterControllerはv2の実owner callbacksと
   MatchHfpTargetを必須、ValidatePcm/CodecLatencyのNULLは未資格として受ける。
   登録失敗時は参照/登録の副作用なし、成功時は親が参照を保持する契約。
2. Audio GetControllerはversion2を認識する。資格NULL/VerifyDmaQuiescent欠落の間は
   factoryを公開しない。登録到着・資格の後発更新とD0起動順について、Audio側の
   明示再接続契約が別途必要。無限retry/timerや成功stubを入れない。
3. MatchHfpTargetはPASSIVE_LEVEL。v1 headerの「match <=DISPATCH」という記述は
   実attach呼出し元と一致しない。Live/ref/derefだけを<=DISPATCHに保つ。
4. 親/Audioの新SYS/INF版はmain予約が必要。旧1007や凍結1008/1010で別SYSを包装しない。

## 物理target照合の具体実装

`hfp_target.c` はIoGetDeviceAttachmentBaseRefで実target PDOを保持し、
IoGetDevicePropertyDataのDEVPKEY_Device_InstanceId/Parent/Childrenを取得する。
同一radio PDO、直接parentがradio ID、または実parent IDがradioのChildrenに存在する場合
だけを受理する。関係を再読出しして撤去/reparentを拒否する。名前/ID prefix/アドレス形式を
仮定しない。深い/公開されない関係は未対応であり、GUID一致へfallbackしない。
取得IDにはMAC等が入る可能性があるため、出力・永続保存せず全allocationを消去する。
BusRelations IRPの自発送信は禁止と一次仕様で確定し、実装には含めない。

一次仕様:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-query-device-relations
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetdevicepropertydata
- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-parent
- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-children
- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-instanceid

これは実PnPで関係propertyが取れるかのWindows実行結果ではない。HFP GUID membershipの
既存確認と組み合わせる実照合sourceであり、標準stack接続成功とは別。

## E宛REA依頼（1件、main中継可）

公開vendor0c756f4のroot内 `vendor-reference/libbluetooth_mtk.so`
（SHA256 `77e897889ea40a09ced2baf0194b92edc97720c963c7a484a8529962e7bea429`）を対象とする。
FC72 command emissionからGORM bytes8..11のPCM rate/word幅/frame/極性/master-slave設定、
merge DAI pad選択時の値の意味を特定し、関数/offsetと参照fieldを返してほしい。
必要なら同manifestのpure/vendor SOを参照する。個体NVRAM record/MAC/校正値を出力しない。
依頼はFC72のみ。Windows未公開GET_DEVICEOBJECTの解析を同じ依頼へ混ぜない。

Raman/Eの子担当はこのsessionのlist_threadsに出ず、mainへのsend_message_to_threadも
native ancestor制約で拒否された。送信済みと偽らず、このfileをmain中継用とする。

## SD trial47への一行handoff

凍結SD3にはCID/CSD容量・READ完了の直接UART出力なし（CID serialは非出力）；実READ完了は
SD `scsi.c:152–153`→`windows.c:171–175`、main側実READ status/転送byte数のUART結果が必要で、
CM成功だけではRW成功としない。凍結SD source/署名tupleは変更しない。


### Controller到着順の通知

BTはprivate `A04BtHfpControllerReadyGuid` を実BGF PDOへIoRegisterDeviceInterfaceし、
**親への実RegisterController成功後だけ**IoSetDeviceInterfaceState(TRUE)する。
KMDFの自動start-time enableは使わない。StopはFALSEで通知を無効化してから
親登録/参照を解除する。実HFP holdが残る間は解除/電源断を拒否する。
GUIDの存在はtransport owner登録のみで、HCIBypass/PCM/DMA資格ではない。

RamanはこのGUIDにINCLUDE_EXISTING_INTERFACESで登録し、到着callbackからPASSIVE workを
一回queueしてGetControllerすることで、Audio先行/BT先行の両方を扱える。
撤去callbackもinlineで待機せずowner rundownをqueueする。資格欠落の状態は明示未対応、
timer retryを足さない。通知link自体はruntimeにのみ保持し、ログへ出さない。

f6b7258の初回WDK compileでIoGetDeviceAttachmentBaseRefの宣言header不足を検出したため、
公式DDI指定どおりBT-private matcher headerをntifs.hへ修正した。SDKやDDI定義を自作せず、
同commitからSYSは生成されていない。INF2004の最終buildは後続固定sourceを用いる。
