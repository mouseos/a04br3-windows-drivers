# MMA I2C1単一ownerを保つHI556共有候補

基点は署名/稼働MMA0.0.1.8のmanifestが指す`2eea34ede8c18b4a3093de255b4f7ace9a695d0b`。
mainの旧MMA0.0.1.0を共有化の基点にすると、識別文字列、portrait軸、hardware counter、
完了優先deadline、idleだけの復旧が失われるため、その実績版を明示的に取り込んだ。
MMA read/axis/HID coreの6filesは基点と同じblobであることをbuild helperが強制する。
新候補INFは0.0.2.0とし、稼働版と出自を区別する。

## 実装した経路

`A04_MMA_SHARED_IDENTITY=1`のMMA providerだけがquery-interface/device-interfaceを公開する。
既定0では追加interface/分岐はコンパイルされず、基点のMMA動作を維持する。

1. Windows kernel clientは`IoGetDeviceInterfaces`で唯一のproviderを選ぶ。
   `IoGetDeviceObjectPointer`の参照を保持して標準`IRP_MN_QUERY_INTERFACE`を送り、
   成功したINTERFACE参照を保持する。close時にInterfaceDereference。
2. providerはMMAと同じWDFWAITLOCKを取得する（新clientの待ち上限1秒）。
   online/MMIO/既存HSTAを確認し、未知handoffでは停止する。
3. 参照付きの**実power provider**へAcquireを依頼する。無い場合はSTATUS_NOINTERFACE。
   power leaseはこのlock内で取得/検証/解放するため、MMAのtransferと同時に
   camera mux/powerを変更する経路は作らない。
4. 同じI2C1 mappingでHI556 main0x28の0f16/0f17を各pointer STOP/read STOP。
   全4transferを同じlock内で完了する。MMIOや任意address/write APIをclientへ渡さない。
5. ACK error/timeoutは失敗。転送完了とSTART idle、RX FIFO実byte数が揃うまで成功にしない。
   completionをelapsed checkより先に確認する実績MMAの規則を維持。
   各transfer最大2000poll/100ms判定、無限retry/resetなし。
6. power lease解放成功後だけ成功を返す。STARTが残る/解放が失敗する場合は
   tokenとpower provider参照およびMMA owner自身の参照を保持して隔離し、MMA poll/D0再開/MMIO解放を拒否する。
   不十分な解放を成功にしない。隔離はreboot/recoveryで解消する対象で、自動再試行しない。

`online=0`後のD0ExitとReleaseHardwareは同じlockで進行中batchを排出する。
opt-in版timerもlock取得後にonlineを再確認する。contextの寿命はINTERFACE参照で保持し、
PnPで資源が解除された後の新要求はMMIOを触らない。
既存MMAのHID識別/軸変換/既存D0設定は変更しない。

## 標準APIの選択

SpbCxを既存の直接MMIO providerと並行稼働させることはできない。
SpbCx 1.x controllerとしてのACPI接続resource/一括移行無しに二つ目のcontrollerを
登録する変更は避けた。今回は既存BU21025の親query-interface参照方式と同様の
標準PnP INTERFACEを使い、MMIO所有者を一つに保つ。SpbCx互換を広告しない。

## 電源/clockとframe側の境界

`power_sequence.c`は正規vendorのHI556分岐に沿ってreset assert、PDN off、
VCAMIO1.8V/VCAMA2.8V/VCAMD1.2V、24MHz MCLK、PDN operating、1ms、reset解除、1msを処理する。
各callbackは実共通resource ownerの参照付き操作でなければならず、失敗はuncertainに残す。
GPIO番号をMMIOで直接叩く実装、他consumerを落とすrail off、未証明のMCLK bit書込みは無い。

**現Windowsにはこのcamera power/clock resource providerがまだ無い。**
現HSTAはI2C1/MMA handoffだけで、camera rail/MCLKの証拠にはできない。
本候補は共通I2C経路と電源順序を実装しているが、現firmwareでcameraをbind/streamできない。
前面ICも未知であり、二つ目のHI556は登録しない。

frame receiverは別moduleへ接続する必要がある。背面HI556のLinux実績はstream-on/NCSI2到達
までであり、CAMSV/DMA非ゼロframe未成功。今回AVStream pin、CSI DMA、frame成功stubは追加しない。
OTP/校正は未読であり、固定値/別個体dataを組み込まない。

## WDK8.1 ARM build

```powershell
# 既定off版。shared interfaceを追加しない。
& .\tools\ci\build-mma8652-shared-identity.ps1 -OutputDirectory "$PWD\build\mma-shared-off"
# 明示opt-in版。real power providerなしではidentity I/Oを拒否する。
& .\tools\ci\build-mma8652-shared-identity.ps1 -OutputDirectory "$PWD\build\mma-shared-on" -SharedIdentity
```

実績MMA6core blob、source40hex/clean、WDK8.1/KMDF1.13、ARM scalar flagsを検査し、
MMA provider SYS/INF/CATとcamera client/power/identity libraryを生成する。
camera libraryは非配備用no-entry native imageでもlinkし、kernel importの未解決を検査する。
署名する場合は既存certのthumbをCertificateThumbprintへ明示する。生成・trust変更はしない。
root内の専用固定sourceから実buildし、回収manifestとhashを記録する。
単独実機試験は行わず、配備可否は未成立power providerと統合結果を含めて判断する。
