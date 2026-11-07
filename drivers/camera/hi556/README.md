# HI556 背面カメラの最小source port準備

> 後続の統合RAW候補は[CAPTURE-CANDIDATE.md](CAPTURE-CANDIDATE.md)を参照。
> 以下はidentity/shared-owner段階の履歴で、未実装との記述は当時の状態。

## センサー調査の結論

A04BR3専用DTSとdefconfigでは、追加のALS/近接/gyro/磁気/気圧ICを特定できない。
`mt8167.dtsi`の`als_ps@0`等はI2Cアドレス/機種固有設定を持たないgeneric node。
status省略をdisabledと読み替えもしないが、これを物理ICの実在証拠にはしない。
A04BR3 defconfigの具体的な小センサー設定はMMA8X5Xだけであり、MMA8652は既に移植済み。
各CUSTOM_KERNEL_ALSPS/GYROSCOPE/MAGNETOMETER/BAROMETERはdefault無指定boolで、
機種defconfigでは選択されていない。XNUの現機種packageにも追加IC実装は確認できない。
これは「物理的に絶対存在しない」という証明ではない。架空のHID Sensorは追加しない。

## 実モデルと出自

- 正規vendor source [CT3_GPL_LGPL_Src](https://github.com/s1204IT/CT3_GPL_LGPL_Src)、
  固定`7c4782127d4ea11965be00ce1343056be9bfb769`。
- `kernel-4.4/arch/arm64/boot/dts/mediatek/a04br3.dts`：mainはI2C1/0x28、
  reset GPIO49、PDN GPIO48。I2C0にはgeneric camera_sub@28もある。
- `kernel-4.4/arch/arm64/configs/a04br3_defconfig`：
  `CONFIG_CUSTOM_KERNEL_IMGSENSOR="hi556_mipi_raw"`だけを選択。
- `kernel-4.4/drivers/misc/mediatek/imgsensor/src/mt8167/hi556_mipi_raw/hi556_mipi_Sensor.c`：
  wire write-ID0x50=7bit0x28、16bit big-endian register pointer、
  0x0f16/0x0f17を各1byte読取り、ID0x0556と照合。
- 同`camera_hw/kd_camera_hw.c`：HI556用VCAMIO1.8V/VCAMA2.8V/VCAMD1.2V、
  MCLK、PDN/reset手順。別モデルの電源分岐は使わない。
- 実基板Linux移植 [a04br3-linux-drivers](https://github.com/mouseos/a04br3-linux-drivers)、
  固定`3d2c74e252d6f34a5980deda96bdc04064108d74`の
  [camera-bringup.md](https://github.com/mouseos/a04br3-linux-drivers/blob/3d2c74e252d6f34a5980deda96bdc04064108d74/docs/camera-bringup.md)と
  [実測監査](https://github.com/mouseos/a04br3-linux-drivers/blob/3d2c74e252d6f34a5980deda96bdc04064108d74/docs/camera-self-audit-2026-09-01.md)：
  背面HI556がchipID0x0556を返し、stream-on/NCSI2到達まで観測。
  非ゼロframe/DMA画素取得は未成功。Windowsでの成功証拠ではない。

**前面ICは未確定**。Linux履歴はgeneric sub nodeへ第二HI556を誤登録して共有railを
余分にpower-cycleした問題を明記し、その登録を撤回している。同じ型番を推測で補わない。

## 今回のコードと安全境界

`hi556_identity.c`は固定モデルのID読取りだけ。既定offでcallbackは一つも呼ばない。
明示opt-in時にもmain I2C1/0x28だけを要求し、0f16のpointer2byte→STOP→read1byte→STOP、
0f17の同手順を使う。board Linuxの`hi556_read_reg()`と同じtransfer形態であり、
combined WRRDへ変換しない。address pointer以外のセンサー値write APIはない。
実byte数、各境界のlease liveness、ID値、finish結果を確認し、不十分なI/Oを成功にしない。
retry/scan、GPIO、PMIC、clock、stream、CSI/DMA書込みは追加しない。

`a04_hi556_io`は将来ownerの**内部契約定義**であり、実装済みWindows interfaceではない。
beginはPASSIVEで、共通ownerによるpowered/clocked main-camera leaseとI2C1全体のlockを
取得する必要がある。address_stop/read_stopは有限timeout付き同期I/O、finishは実drainを
伴うlock解放とし、失敗時は所有資源を隔離する。ローカルmutexやbooleanだけでは代用不可。

現在MMA8652がI2C1を専有している。MMAを迂回する第二MMIO ownerを作らない。
共通ownerへの移行、電源rail/MCLKの参照寿命、PnP/電源rundownが未成立なので、
SYS/INF/ACPI deviceは追加せず、bind不可のsource coreに留める。既存MMAのHID Sensor
識別契約は参考にしたが、カメラをHID Sensorとして公開することはしない。
将来のカメラ経路はKS/AVStream/CSI受信と実DMAが必要。
OTP/校正は未読で、固定値や他個体データを組み込まない。モデルID読取りは個体IDではない。

## 再現build

WDK8.1 ARM/clang/lldの既存Windows環境で固定clean sourceから実行する。
省略時はdefault-off object/library。明示switch時のみidentity coreを有効にする。
これはインストール可能なドライバではない。自動実機試験は行わない。

```powershell
& .\tools\ci\build-hi556-source-core.ps1 -OutputDirectory "$PWD\build\hi556-disabled"
& .\tools\ci\build-hi556-source-core.ps1 -OutputDirectory "$PWD\build\hi556-optin" -IdentityReadExperiment
```

manifestへsource/recipe/header/object/archiveのhash、実モデルとblockerを記録する。
今回のhost確認は既存形式の単一fixtureでdefault-off無操作、正しいID transaction、
short-read、lease失効、finish失敗を確認する。ARM32 COFF/scalar compileは実I2C測定の代替ではない。

## 後続の共有owner実装

実績MMAを基点にした実kernel query-interface/STOP transport/電源順序moduleとWDK packageは
[SHARED-OWNER.md](SHARED-OWNER.md)を参照。旧source-only coreに加え、唯一のMMIO provider側へ
共有経路を実装している。power providerが未接続の現構成ではcamera bind不可を維持する。
