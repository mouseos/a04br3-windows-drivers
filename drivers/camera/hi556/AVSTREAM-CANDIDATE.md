# HI556 AVStream capture接続候補

## 実装と未完了を区別する

RAW owner0.0.1.3/schema3と、別AVStream package0.0.1.0を一組とする。
標準KS capture pinから既存ownerの実sensor/receiver/DMA取得まで接続する実装であり、
RAWを合成画像で代用するsampleではない。Windows実機のframe/DirectShow/WinRTアプリ動作は未確認。
**実HI556のNCSI2→MUXのno-frame根因は資料だけで確定しておらず、撮像完成は未達。**

## 根拠と採用しない変更

CT3 `7c4782127d4ea11965be00ce1343056be9bfb769`のHI556はNCSI2、2-lane(enum1)、
packet ECC order1、RAW_Gb、preview1296x972。global/preview tableは既存の固定sourceを維持する。
MT8167 seninf_drv.cppのsetSeninf1NCSI2は実CTLをRMWしてHSRX_DET bit7をclearし、
REF_SYNC bit26/ECC bit16/clock bit4/data bit1:0を設定する。
SW_CTRL_MODE bit0をclearする時もCAL3C=0x1541を維持し、HW_CAL_START bit2ほかを丸書きで
潰さない。10011000 analogの新たな設定やlane/settle総当たりは追加しない。
通常setterにないdebug selector11/45/12のwriteをdata-pathから除き、既存selectorを保存する。
これらはsourceへの整合であり、以前のLinux試験でno-frameを解消した実績とは主張しない。

照合した既存一次source（読取りのみ、再配布しない）：
`MT8365_Q0_Ayla_bsp/vendor/mediatek/proprietary/hardware/mtkcam/legacy/platform/mt8167/hal/sensor/seninf_drv.cpp`、
SHA256 `1ae38768c2a5e66495074a90fc92bc7aa4e5be16adf14ced60366ece2cf4e691`。
別SoC MT8168のenumやレジスタ配置へ置換しない。出荷binaryの新しい逆解析は実施しない。

Linux正本commit3d2c74eと、未commitの後続docs/試験を区別した。
後続のcamera-t12-raw10-packing.mdでは**SoC内部TMのみ**実DMA/全byte取得・packing一致を確認。
実HI556ではT13/T16を含め画像未取得。既存試験で否定されたMUX source0、lane増加、settle変更、
CAL保持だけの再試験、MUX reset省略、test-model成功を実sensor成功とする案は採用しない。
根拠不足のMUX_SIZE writeや第二SPM/INFRA ownerを追加しない。

## 実DMAと画素形式

同じLinux記録のIMGO出力は4sample/5byteの**little-endian連続10bit詰め**。
RAW ownerのformat2はこの未変換DMA形式。旧format1をMIPI RAW10としてdecodeしない。
padding10byte/lineは画素から除外する。全有効sampleを読んでnonzeroかつmin!=maxを要求する。
真っ暗/固定値の実景も保守的に拒否し、単なる長さ/IRQ/sentinel変更を撮像成功としない。

標準出力は2x2 GBRGセルの実R/B、2つの実Gの平均をRGB24へ変換する。
648x486、stride1944、bottom-up BGR。WB/色行列/OTP/3A補正や個体の固定値は無い。
これはISP品質調整を完成したカメラではなく、標準captureへ実pixelを渡す最初の形式。

## AVStreamの所有と失敗

- WDK8.1 KsInitializeDriver、pin-centric PASSIVE processing。ks.h/ksmedia.h実headerでcompileする。
  根拠：[Microsoft AVStream sample](https://github.com/microsoft/Windows-driver-samples/tree/main/avstream/avssamp)。
  現行sampleのWin10 Camera class/dirid13は使わず、8.1 Image class/dirid12とks.inf/kscaptur.infを使う。
- ROOT\A04BR3HI556Captureのsoftware bridge。ACPI MOU0020のRAW ownerを置換しない。
  DeviceStartで実enabled RAW interfaceが1つだけ存在し、schema3と実ID0556を返すことを要求。
  不明board/owner未搭載は開始拒否。front sensor、未実装control/capは広告しない。
- 標準VIDEO/CAPTURE/VIDEO_CAMERA category、単一固定RGB24 pin。動的format変更、crop、
  raw Bayer subtype、preview/静止画別pin、3AやKS camera controlsを実装済みと広告しない。
- raw deviceはexclusive、AVStreamもpin1つ。第二MMIO/I2C/DMA adapterは無い。
  raw status取得は実ID検証だけで、stream-onや撮像を起動時に実行しない。
- ProcessはKS stream pointerをlockしたまま同期RAW request→結果検証→画素変換→返却する。
  下位は既存32秒servicing deadline＋各ACPI/I2C bound、3秒poll/cancel/停止ACKを保持する。
  STOP/PAUSE要求はatomic flagを先に立て、mutexで進行中の同期request排出を待つ。
  request完了前にbuffer/handle/contextをfreeしない。新worker/DPC/独自brokerは無い。
  KS cancelは取得の前後で確認し、実行中RAW呼出しの途中強制終了を保証しない。
- acquisition failureをそのKS IRPの失敗として返し、DataUsed0を成功frameとして返さない。
  失敗はSTOPまでstickyで、自動の無限再取得はしない。STOP後の明示RUNでのみ再開始できる。
- 最大software cadence1fpsは要求頻度の上限で、hardware実測fpsではない。
  実取得完了時に実KS reference clockがある場合だけTIMEVALIDを設定。
  未測定durationは無効のまま、interrupt-timeを別clock epochとしてtimestamp化しない。

## buildと統合条件

```powershell
& .\tools\ci\build-hi556-capture.ps1 -OutputDirectory "$PWD\build\hi556-raw13" `
  -CaptureExperiment -SharedOwnerDirectory "$PWD\build\fixed-mma21" -CertificateThumbprint EXISTING_THUMBPRINT
& .\tools\ci\build-hi556-avstream.ps1 -OutputDirectory "$PWD\build\hi556-avstream10" `
  -CaptureExperiment -CertificateThumbprint EXISTING_THUMBPRINT
```

すべて既定off、固定clean sourceと新output、全kernel TUはNoFP/NEON。
MMA2.1はsource11blob/criticalflags/CAT membership/実SYS hashを確認して署名bytesを再利用。
共通EDK/機種ACPI/親driverは変更しない。matched opt-in camera handoff firmwareが依然必要。
Carver thermal1022とのIMG lifetime条件はSHARED-SPM-CONTRACT.md末尾をmain経由で照合する。

実機mainの統合時はRAW ownerの実ID/開始結果、fresh receiver各境界と実frame、
次に標準KSのformat intersection/ACQUIRE/RUN/STOP/再開始、実非固定画像・MMA退行無しを確認する。
WinRTアプリが列挙・形式受理するかは実結果で判断し、SYS署名/KS登録をその成功としない。
主担当の追加根拠が来るまでレジスタ総当たりや同一失敗の単独再試験をしない。
