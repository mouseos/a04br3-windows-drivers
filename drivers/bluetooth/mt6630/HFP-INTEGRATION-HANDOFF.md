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
