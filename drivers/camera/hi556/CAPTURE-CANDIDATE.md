# HI556 統合RAW撮像候補

これは部品libraryだけではなく、電源/MCLK provider、唯一のI2C ownerへのclient、
センサー設定、受信/DMA、明示取得EXEを一つのWDK buildで結ぶ候補。
AVStream/WinRT cameraとしては公開せず、System classの診断RAW取得deviceとする。
**Linux元実装も非ゼロframe未成功で、Windows撮像成功を主張しない。**

## 必須構成

- 機種専用UEFI `work/windows-camera-handoff`。`A04BR3_CAMERA_HANDOFF=1`の時だけ
  MOU0020を追加。ISP reset ACKによる旧DMA停止を含む版が必須。
  shared PLLの再設定を避け、実camera rail ACK/MCLK測定/clock/pin stateを確認して
  NVS markerをpublish。Common EDK2は変更しない。既存通常FDへ無断で混ぜない。
- MMAの`A04_MMA_SHARED_IDENTITY=1`、`A04_MMA_SHARED_CAMERA_STREAM=1`。
  companion INF0.0.2.1。MMAだけがI2C1をmapし、全transferを従来と同じWDFWAITLOCKで直列化。
  稼働MMAのread/axis/HID core6fileは2eea34edと同じblobをbuild時に強制する。
  既定offでは追加経路を除外する。SpbCx controllerを並行登録しない。
- HI556 `A04_HI556_CAPTURE_EXPERIMENT=1`、INF0.0.1.0。
  resourceはISP/SENINF15004000+9000、analog10011000+1000、LARB1+1000、read-only NVS、IRQ212。
  cameraはI2C1/TOP/GPIO/PWRAPをmapしない。NVS+ACPI HSTAと実ID0556の確認失敗でD0拒否。
  前面ICは未知のままbindしない。

## 電源と寿命

firmwareで確立したcamera railと24MHz MCLKをboot中pinする。
Windows providerは参照付きpower interfaceを実装し、NVSとACPI HSTAを確認してtokenを発行。
ACPI呼出しはPASSIVE、1秒timeout。D0/queue rundown後だけ新leaseを停止する。
ReleaseはWindows側参照を減らし、共有rail/clockを推測でoffしない。
これはcameraの省電力完成ではなく、bring-up用の明示pinned電源契約。

I2C register interfaceは既存identity ABIと別GUID。実ID確認済みのmain0x28だけを扱う。
pointer2byte STOP/read1byte STOP、またはreg16/value16の4byte STOP write。
MMAのsampling/軸/識別/電源操作を別controllerで迂回しない。
START idle、FIFO実長、有限timeout、power liveness、解放結果を確認。
不確実なI2C/電源状態ではownerのtoken/refを隔離する。

## 受信とDMA

正規vendor sourceのglobal166call、preview41callを機械的に再現。
preview末尾stream-onだけはreceiver arm後へ分離する。新opcode/未知table/個体OTPは追加しない。
`tools/camera/update-hi556-tables.py VENDOR_ROOT --check`で固定source hashと再現hashを確認。
GPL source tablesのCOPYINGを同梱する。vendor HI556分岐にOTP/個体校正readerは無く、
本候補も架空の補正値を持たない。

receiverはboard Linux3d2c74eのPass1 RAW10経路を移植し、MT2712 CAMSV+9000とは混同しない。
1296×972、GBRG packed RAW10、payload1620byte/line、stride1630。
既存LinuxのNCSI2→MUX停止境界を解消した修正ではない。
棄却済みのregister総当たりやtest-model/別sensor変更は追加しない。

- PnP resourceの唯一性/物理map/サイズ/重複を検証。
- LARB1 IMGOのM4U/securityが既にbypassであることを要求。共有portを勝手に変更しない。
- IoGetDmaAdapter version3→noncached CommonBuffer、32bit logical address。
  CPU物理addressの推測やuser bufferへの直接DMAはしない。
- hardware reset ACKで旧DMA停止を確認してからbufferを確保/初期化する。
  前後4KiBのguardを含めて所有し、frame出力にguardは含めない。
- captureは明示IOCTLのみ。起動/自動service/scan/無限retryは無い。
  IRQ源をmaskしたpoll方式で、INT_ENはwrite-clear bit31だけ。
  fresh Pass1とIMGO両DONE、TG/queue/IMGO/DMA error無しを要求する。
- 3秒monotonic deadline＋最大3000poll。cancel/timeout/errorもsensor stream-off→
  VFDATA/CMOS停止→SOF snapshot→ISP reset ACKを実行する。
  2回目のSOFが来た単一bufferは部分上書きの可能性があるので返さない。
- reset ACK、両DONE、SOF差1、guard保持、sentinel変更を満たす場合だけframeを返す。
  不十分なDMA完了や不確実なstopを成功扱いしない。
  DMA停止不明ならbuffer/adapter/MMIO/power/device参照を隔離し、free/unmapしない。

`--status`は固定status snapshotのみ。capture失敗時にOSがoutput bufferを返さなくても
次のstatus呼出しで実NTSTATUS/ID/受信status/guard等を回収できる。
frameデータ/個体OTP/ポインタをstatus logへ出さない。

## 固定sourceからの一括build

```powershell
& .\tools\ci\build-hi556-capture.ps1 -OutputDirectory "$PWD\build\hi556-candidate" `
  -CaptureExperiment -CertificateThumbprint EXISTING_THUMBPRINT
```

camera SYS/INF/CAT、ARM32 client、companion MMA SYS/INF/CATを作る。
全kernel TUは非FP/SIMD codegenとWDK kernel runtimeを使用する。
新しい固定checkout内outputのみ。source/critical flags/table/recipe/file hashをmanifestへ記録。
runner/実機操作はbuild script自体には含めない。

主担当による明示操作例（現在の通常firmwareでは動かさない）:

```text
hi556-capture.exe --status
hi556-capture.exe --capture NEW-FILE.raw10
```

clientは既存fileを上書きせずCREATE_NEW。実frame取得後にだけRAWを保存する。
成立判定はfresh bootの実ID、2回の異なる撮像/期待geometry/非固定画素、stop/restartと
MMA/他deviceの退行無しを含める。library/driverのbuild成功やNCSI2到達だけでは完了しない。

未完了はWindows実非ゼロframe、元Linux停止境界の解明、AVStream/WinRT camera公開、
camera rail省電力、前面IC識別。実機を所有する主担当の統合試験結果を待ち、同じ盲試験を増やさない。
