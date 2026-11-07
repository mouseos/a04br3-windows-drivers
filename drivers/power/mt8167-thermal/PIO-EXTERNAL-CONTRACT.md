# AUXADC PIO実装と測温再開の外部境界

## 実装済みの範囲

auxadc_pio.cはMT8167 AUXADC channel11をCPUの16bit MMIOで変換する。
Linux IMM_auxadc_GetOneChannelValueのCON2 busy確認、CON1 trigger clear、
DAT11 old ready=0、trigger set、25us待機、new ready=1を実装した。
各pollは32回で打ち切り、timeoutをOS panicに変換しない。thermal controllerの
自動AHB program・IRQ・SPM hot threshold・DMA/host backingは生成しない。

全出口でleaseは呼出元のまま。sample失敗時もtrigger clearとbusy=0を確認し、
局所ADC idleを確認できなければRETAIN＋quarantineを保持する。
write失敗は到達不明なのでtouchedを先に記録、未証明の再試行を禁止する。
局所idleはIRQ/fabric/firmware drainやlease返却の証明ではない。
raw値は新ready edgeとcleanup両方が成功した場合だけ出力し、失敗時に旧値を
温度として公開しない。OS時刻や固定値を温度に使わない。

これは変換の実装であり、3 sensor温度のproduction経路ではない。
TS_CON1によるsensor選択、buffer/settleと実校正を担うqualified ownerが未接続。
WDF backendへ無資格で接続・enableするflagは追加しない。既存拒否guardは維持する。

## 一次ソース（CT3固定7c4782127d4ea11965be00ce1343056be9bfb769）

- kernel-4.4/drivers/misc/mediatek/auxadc/mtk_auxadc.c:281-379。
  mutex_get_cali_valueはこのLinux ADC APIのmutexであり、WindowsやEL3の排他ではない。
- 同auxadc/mt8167/mtk_auxadc_hw.h:59-72。CON1=+004、CON2=+010、DAT0=+014。
  DAT11=+040。ready bit12、data mask0xfff。CPU APIは16bitアクセス。
- 同thermal/mt8167/mtk_ts_cpu.c:787-814。自動controllerはADC trigger/dataと
  TS_CON1の物理アドレスをAHBへprogramする。CPU pollだからbus master不使用とは言えない。
- 同:676-724。ISRはTHERMINTST/TEMPMONINTSTSを読む。明示的W1C ackはこの経路に
  見つからず、読出し副作用を推定してclear/drain実装にしない。
- 同:3113-3206。AHB idle poll、pause、periodic disable、clock off。
  Linuxの50回打切り後の継続をWindows失敗時の解放資格として流用しない。
- kernel-4.4/arch/arm64/boot/dts/mediatek/mt8167.dtsi:902-910。
  thermal IRQはSPI77→GSI109、level-low。Windows親thermal子は現状2 MMIOのみ。
  資源/ISR/DPC同期なしにIRQ drain資格を宣言しない。

## main/Ramanへ必要な具体契約

1. **実ADC所有移譲**。現341894cd機種ThermalHandoffDxeはBeforeEBS callbackが
   実行された観測だけを渡し、record ownership=0。EL3/LK/別ADC利用者を停止しないと
   source自身が明記。必要なのは全利用者停止とOSへのexclusive移譲を行う実producer。
   boot record値変更やparent native_claim=1、reset readback、Linux mutexでは代替不可。
2. **初期hardware資格**。既存thermal masterが停止し、ADC自動mode/channel triggerが
   inactive、既存IRQがmask/drain済みである証拠。PIOは新IRQをenableしないため新規
   ISR/DPCを作らないが、以前のownerが残したpending状態を無視しない。
   clockがgateされた状態の危険なMMIO読出しで観測を作らない。
3. **supplier側quarantineと寿命**。parent-owned記録で通常操作を拒否しつつ、ADC/analog/
   clockへの必要な保持を子context・戻り値に依存せず維持する契約。StopIdleはsleep/
   surprise/removeの保証でなく、ReleaseHardware失敗でもPnP resource/code lifetimeを
   保持できるとは言えない。純PIOではDMA backingを作らないが、共有hardwareを
   未停止のまま別利用者へ返すことはできない。
4. **sensor identity/settle**。TS_CON1 selector0/1/2と各校正sensorの対応、analog変更の
   読戻し/settle、全終了経路のrestoreと競合排除。generic ADC rawだけを3個複製して
   3 sensor温度成功にしない。

これらの実producerは現在source内にない。従って機能するthermal署名候補の生成は
この外部境界で未完了。拒否専用SYSを再署名して測温候補と呼ばない。
共有親を変更する場合はmain/Ramanと新parent版を予約し、1010/1012凍結SYSを変更しない。
brightness/SD/BQ署名候補は別成果物で、その適用をthermalに依存させない。
