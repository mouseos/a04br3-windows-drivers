# MT8167 USB20のboot keyboard接続

基点は同一drivers repositoryの`c027fa3`。LED実装を維持し、共有親の
電源所有、USB用PDO、HIDClass transport、MUSB起動、ポート状態管理を
WDK 8.1でまとめてビルドする構成を追加した。実機とSSH runnerは操作していない。
これは直接接続のboot keyboard経路で、移植全体の完了ではない。

## 実装した接続

| 段階 | 接続 |
| --- | --- |
| 実PnP | `UsbKeyboard.asl`の有効なACPI BEN0001に共有親INFがbindし、親が`MT8167\USB20`のPDOを1個生成。MAC/SIF各64KiB、GSIV104を要求。USB専用profileでは他の試作PDOを生成しない |
| HID | 既存のmshidkmdfとKMDF lower filter。HID READ_REPORTへ8byte入力を配送し、既存GET_INPUT_REPORTとLED SET_REPORTを維持 |
| 給電・GPIO | 親の排他ロック下でGPIO22をlowにしてGPIO mux/outputを設定。GPIO34 input、ID41 input/IDDIG mux2、MT8167のIES SET aliasを設定。外部VBUS検出中やID非hostではsourceを開始しない。MT6392 VUSBと既存clock lease経由で起動 |
| Clock | UNIVPLLの稼働・電源・分周を読取り検証。USB20とUSB11の両consumerがgatedのときだけ、USB78 muxの該当3bitをUNIVPLL/16へ変更。他PLLは設定しない |
| PHY | 既存MT8167回復列にfrequency meterによるslew校正を接続。最大3秒、Linuxと同じtimeout fallback=4。計測値0や範囲外では後処理して失敗 |
| MUSB | 既存VBUS/session開始順を維持しPOWERのENSUSPEND/HSENABを設定。PHY操作前とIRQ arm前に8個のHSDMA channel ENABLEを検査し、未知のDMAを勝手に停止しない |
| 列挙・入力 | CONNECT後100ms debounce、50ms reset、10ms recovery、EP0列挙、interrupt-INのPIO pollを`keyboard_port.c`で連続管理。列挙の無進行・RX応答なしは2秒で失敗。切断時にrelease report、EP0とRX FIFO清掃、同じ給電session内の再接続を処理 |
| 異常終了 | ISR同期でmaskし、worker自身をflushせずにsession/VBUSを停止。D0/releaseで残りのDPC/workをdrainしてからunmap。故障状態からの無根拠な自動再開はしない |

Linuxの`usb20_host.c`では`CONFIG_OC`が無効なので、このprofileではGPIO21を
過電流測定値だと仮定しない。従来必須だったGPIO21のチェックを、当該profile
ではID/source healthへ置換した。**過電流保護を実装・確認したという意味ではない。**
BQ24160のboost設定や未確認のレジスタ書込みは追加していない。

## 同型番一次ソース

基準: `s1204IT/CT3_GPL_LGPL_Src` commit
`7c4782127d4ea11965be00ce1343056be9bfb769` の `kernel-4.4/`。
ローカルも同commitであることを確認した。

- `arch/arm64/boot/dts/mediatek/a04br3.dts`: GPIO22 VBUS、GPIO41 IDDIG、GPIO34 USB detect、MT6392 VUSB。別boardのGPIO番号を使用しない。
- `arch/arm64/boot/dts/mediatek/mt8167-pinfunc.h`: GPIO41のIDDIG mux値2。
- `drivers/pinctrl/mediatek/pinctrl-mt8167.c`: DIR/DOUT/DIN/MODE配置、port shift=4、SET/CLR間隔=4、IESの0x900 bit2（34..39）とbit10（41..43）。
- 同`pinctrl-mtk-common.c`: directionは16pin/bank、MODEは5pin/register×3bit、SET/CLR aliasとmasked updateの計算。
- `drivers/clk/mediatek/clk-mt8167.c`: USB78 mux offset4 bits22:20・parent2=UNIVPLL/16、USB/USB1P gateとPLL配置。
- `drivers/misc/mediatek/usb20/mt8167/usb20_phy.c`: `usb_phy_recover`と`hs_slew_rate_cal`。PHYから見た0x700台はSIF+0xf00のfrequency meter。
- 同`usb20_host.c`: GPIO source、sessionのidle→host列、100ms/5ms待機、`CONFIG_OC`無効、GPIO入力によるID判定。
- `drivers/misc/mediatek/usb20/musb_core.c`: `musb_start`のPOWER設定。
- 同`musb_host.c`: EP0 FIFO清掃、RX FIFO二重flush。
- 同`mtk_musb_reg.h`と`musbhsdma.h`: CSR/DEVCTL/HSDMA bitとoffset。

## 主担当が行う次工程

主担当の既存WDK 8.1環境で、固定commitから次を実行する。スクリプトは
runner起動、SSH、署名、インストール、書込みを行わない。

```powershell
./tools/ci/build-mt8167-usb-keyboard.ps1 -OutputDirectory ./build/usb-keyboard
```

親SYS、`mt8167-usb20.sys`、対応する2個のINF、ACPIソース、出自とhashの
manifestを一緒に出す。旧scriptの`mt8167-usb20-hid-transport.sys`という
生成名とINFが要求する`mt8167-usb20.sys`の不一致も修正した。
出力とTEMPはcheckout内に限定し、環境変数は終了時に元へ戻す。

主担当は既存の署名・媒体生成工程で2個のcatalogを作り、**同じ構成の親と子**を
組み込む。`UsbKeyboard.asl`は既存BEN0001や同じ共有MMIOを所有するlegacy
ACPI nodeと重ねない。単に追加すると排他resource競合になる。既存構造を
このサイド作業では変更していない。GSIV104はDTSのSPI72 level-lowであり、
Windows resource requirement単独では極性を指定できないため、機種の既存
SYSIRQ設定とHALへの割込み記述を主担当が一組として確認する必要がある。

実機で確認する到達点は、親/子のPnP開始、PHY/clock/source条件の成立、
CONNECTと列挙完了、HIDClass経由の実キー入力とキー解放、再接続、LED出力。
ドライバはDbgPrintに`MT8167 USB20 keyboard enumerated`、最初の入力受信に
`MT8167 USB20 first keyboard input report received`を出す。これはカーネル
デバッグ出力で、現状のUART回収へ自動転送されると保証しない。主担当の
既存診断手段で回収し、最終的にはアプリへ届く実キー入力で判定する。

## 現時点の検証と限界

変更範囲を一括したhost確認、USB専用PDO/resource生成の既存WDF shim確認、
ACPIコンパイル、ARM32 core COFF生成まで実施した。新しいポート確認は1本に
まとめ、実装した接続→列挙→入力→切断→再接続と期限超過を同じ流れで扱う。
これは実WindowsのPnP、HIDClass、Driver Verifier、電気特性の検証ではない。
今回WDF翻訳単位をWDKでコンパイル/リンクしたという主張はしない。

汎用Windows URB、USBPORT miniport登録、root hub、hubや任意のUSB deviceは
未実装。WDK 8.1の公開DDIが未確認であるため、名前だけURBとしたstubや
常時拒否APIは追加しなかった。実キーボード入力へは上記の直接HID transportを
用いる。実機給電・PHY・IRQ・実PnP成功、署名済みパッケージの受理は主担当の
未実施工程であり、USB移植完了として扱わない。
