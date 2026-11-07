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

## 通常OSを維持する統合profile（audio 08faae5との統合）

`98775b4`のUSBと`08faae5`のfirst playbackを、履歴を維持して同じ
`wdf_parent_probe.c`へ統合した。親SYSを2個インストールしない。
主担当のWDKビルドで確認されたUSBのGS helper link errorも同じ変更に含める。

音声とUSBを同時に組み込む場合は、次の**単一parent**ビルドを使う。

```powershell
./tools/ci/build-mt8167-first-playback.ps1 -IntegratedUsb -OutputDirectory ./build/integrated-usb-audio
```

出力は`a04-msdc-parent-probe/mt8167-shared.sys`が1個、`audio/mt8167-audio.sys`、
`usb/mt8167-usb20.sys`と対応INF・hash manifest。parentを先に一度だけ
ビルドし、両childが同じソース・audio interfaceとUSB interfaceを使う。
USB単独の親で、稼働中のfirst-playback用親を上書きしない。

既存の通常parentへUSBだけを追加する場合の入口は次。

```powershell
./tools/ci/build-mt8167-usb-keyboard.ps1 -Profile Integrated -OutputDirectory ./build/integrated-usb
```

`Integrated`が既定。`USB_ONLY=0`、button D0とtouch D0を有効にし、USB関連
以外のaudio/thermal/Wi-Fi等の既存環境フラグは勝手に0へ戻さない。
音声も必要なら上の`-IntegratedUsb`入口を用い、既存の有効な任意consumer
フラグはmanifestと照合する。単独診断の旧構成は明示的な`-Profile UsbOnly`で
選ぶ。統合profileは`UsbKeyboard.asl`を出力せず、既存ACPI parent、touch、
battery、ボタンのnode/INF/SYSを削除・交換しない。

### 既存資源maskと所有範囲を変えない方法

- parentの既存TOPCK/INFRA/GPIO/PWRAP各4KiBは従来の親だけがmapする。
  childに同じ範囲を追加しない。USB clientは親へのquery interfaceを使う。
- `USB_ONLY=0`では、従来の4領域をそのまま受け付ける。APMIXEDがなければ
  USB/audioを利用不可にするが、parentのD0・touch・ボタンの開始は拒否しない。
  OSが割り当てていないAPMIXEDを固定アドレスで勝手にmapしない。
- 既に5領域ならAPMIXEDの既存長`0x710`/`0x714`を維持し、拡張しない。
  USBは`0x710`で足りる。first playbackは`0x714`がなければaudio leaseを
  拒否し、入力の開始とは分離する。
- USB PDOはMAC `0x11100000/0x10000`、SIF `0x11110000/0x10000`、GSIV104
  だけを所有する。touch PDOのI2C0 `0x11009000/0x1000`、ボタンPDOの
  `0x10002000/0x1000`を変更しない。既存USB nodeが同範囲を所有している
  場合はそのUSB nodeだけと排他にし、入力やbattery nodeを無効化しない。
- batteryのLC709203Fが持つI2C2 `0x1100b000/0x1000`には触れない。
  USBにBQ24160/I2C2の別mapping・timer・charger書込みを作らない。
- GPIO更新はUSBのMODE22/34/41とdirectionの該当bit、IES SETのbit2/10に
  限る。既存touchのMODE `0x3b0/0x310`、DIR0 bit8、IES SET bit3/15の
  allowlistを変更しない。bank丸ごとの値を書き戻さない。
- TOPはUSB gateのbitだけをSET/CLRし、muxは`0x004[22:20]`だけを更新する。
  audio gateやtouch/I2C gateのmaskは拡張しない。共有PLLを再設定しない。

### PC接続中の期待状態

外部VBUS検出またはID-deviceなら、親のUSB ownerは**読取りだけのeligibility
probeで、GPIO・clock・PMIC・PHY変更前にacquireを拒否する**。これは故障では
ないのでUSB/PMIC/parentをpoisonせず、touch/ボタンや保持中audio leaseを止めない。
USB childがhost開始不可でも、親全体の開始失敗として扱わない。
後で主担当が別条件で再試験できるが、この作業から接続変更を要求しない。

### WDK8.1のlink error修正

主担当の`windows-rt-usb-audio-batch-build-20261107.log.stderr`にある
`__security_push_cookie / __security_pop_cookie`未解決を確認した。
USB scriptの前半C coreと実験core/contractにも`-fno-stack-protector`を揃えた。
さらに、実ログは`ntoskrnl.lib(helpexcept.obj)`からの参照なので、時刻の
64bit除算が`__rt_udiv64`を引き込む経路も除いた。時刻変換とPHYの整数除算は
32bitのshift/subtractで同じ商を計算し、USB coreのARM出力で当該GS helperと
除算helperの非参照を確認する。既存`bufferoverflowfastfailk.lib`のリンクは維持し、
ライブラリを追加して解決済みと扱わない。

親の前回WDK成功は主担当の結果。今回の統合版SYS再リンク、通常OS上での
touch/battery/ボタン継続開始は主担当の未確認工程であり、ホスト確認から
実機成功へ読み替えない。

## 列挙の追補

`d9d95dc`以後の追補では、EP0の初期packet sizeをhigh-speedで64とし、
low-speedのdescriptorが8、high-speedが64であることをSET_ADDRESS前に
確認する。full-speedは8/16/32/64をdescriptorから取得する。
同じ端末GPL sourceの`drivers/usb/core/hub.c`にある`hub_port_init`の
速度別初期値を根拠とし、今回の限定hostが扱えないサイズは拒否する。

configurationはboot keyboard endpointを選んだ後も末尾まで長さを検証し、
壊れた後続descriptorで途中の選択結果を確定しない。Linux
`drivers/usb/core/config.c`の全体走査と同じく、descriptorの境界を確認する。
EP0/RX/入力配送で失敗したsessionはKB_ERRORへ遷移させ、KB_READYのまま
次の受信要求を出さない。これらは既存fixtureへまとめて確認を追加した。
親・資源・GPU/DOD・installerには変更を加えない。実機成功は未確認。

## Native WLANとの統合

Wi-Fi runtimeを含む単一parent構成とcommit一覧は
`drivers/bus/mt8167-shared/WIFI_USB_AUDIO_INTEGRATION.md` を参照。
`build-mt8167-first-playback.ps1 -IntegratedUsb -IntegratedWifi` でUSB/audio/Wi-Fiを
同一checkoutからbuildし、parentの二重生成とUSB-only ACPIへの置換を避ける。
