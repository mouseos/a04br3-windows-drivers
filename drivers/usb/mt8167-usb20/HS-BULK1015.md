# HCD1015：direct root-port HS bulk 512-byte PIO

親1115 df7892efを基点とする別branch。既署名1115/HCD1014・run139/141 tuple/mediaは変更しない。service MT8167MUSBHcdを保持し、leaf INFを2026.11.8.1015へ更新する。親ソース・ABI・guardは不変更。

1014ではOpenが全packet>64とHS bulkを拒否し、PIOもFS bulkしか扱わない。新規実装はUSBPORT Open→driver-owned shadow→単一PASSIVE worker→EP1 PIO→USBPORT PollEndpoint完了まで同じ経路でHS bulk512を扱う。FS bulk8/16/32/64、従来interrupt<=64を保持。HS bulkの非512値、LS bulk、TT/split、ISO、high-bandwidth transactionは拒否する。未知callbackは追加しない。

Linux根拠は https://github.com/s1204IT/CT3_GPL_LGPL_Src commit7c4782127d4ea11965be00ce1343056be9bfb769。
- kernel-4.4/drivers/misc/mediatek/usb20/mt8167/usb20_host.c:91-92 はEP1 TX/RX各512byte・single buffer。
- musb_core.c:1547-1611 はFIFO size=log2(bytes)-3、address=offset>>3、single buffer割当を規定。
- mtk_musb_reg.h:230-233 はFIFOSZ62/63、FIFOADD64/66。usb20.c:1362 はfifo_size8KiB。

EP0[0,64)、TX1[64,576)、RX1[576,1088)を固定し、FS/HS・方向変更でも非重複を維持する。CSR idleを確認してから幾何を設定し、size6/address8/72を読戻せない場合はpacket command/FIFO data前に拒否。実停止を証明できなければ既存Flush→StopAndProve/retainへ進み、成功を返さない。未知DMA/QMU・OC/ID/給電拒否、16endpoint親停止、500ms role監視、5秒deadline・health/rundownは変更しない。

新runner42479899866の既存小WDK closureを利用し、ARM7TUだけcompile、ep0/ep0_pio/musb_asyncの3objectを固定source/include/hash照合で再利用。親12helperは再buildせず1115署名物を保持。SDK/WDK/PCO全setupなし。

試験はportable ASan/UBSan FIFOモデルと実WDK型のUSBPORT/physicalbackendモデル。複数512packet、IN short/ZLP、OUT ZLP、NAK無重複、stall、overrun、FIFO設定読戻し失敗、HS worker cancelとshadow lifetimeを確認する。実RT8.1登録/標準hub binding・実5V/OC・HS enumeration/実bulk成功・並行PnPは未実測。source完成・署名済みと実機成功を区別する。

残る実装範囲はexternal hub routing/TT・split、ISO、interrupt>64/high-bandwidth、suspend/resumeなど。HS interruptのUSBPORT period正規化とMUSB log intervalの契約は別途確定が必要で、本差分で推測しない。実機main専有、worker非接触。現在PC-roleのadmission拒否は正しいため、この試験結果をhost cableでの転送成功としない。
