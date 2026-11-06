# CPU・USB電源・BQHR 統合候補

`windows-rt-cpu-register-touch-gpu-5341b16` のWIM・ESPとUEFI `e969a116` を基準にする。共有親はCPU観測とUSB観測を同じデバイス・同じD0キュー・同じPWRAPロックに統合した新ビルド。タッチ、GPU、TOPRGUの既存ファイルは保持する。CPU媒体自体も実機未検証であり、署名済み親が実機で受理・開始される保証はない。

起動順はWPE初期化、UART volume listenerとCPU負荷開始、共有親、タッチ、USB観測、BQHR診断ドライバとBQHR観測、CPU観測、GPU、display。各drvload成功はPnP稼働やUART受信成功を意味しない。TOPRGU UARTは複数クライアントで共有し、転送成功はキュー投入までである。観測行欠落・混線は不明として扱う。VCDTの新規変換は実行しない。

USBは同じIDの `A04USB SNAP` 4行。1はGPIO DIR1/DOUT1/DIN1/MODE21_22、2はDIR2/DOUT2/DIN2/MODE34、3はMODE41、4は `valid CHRSTATUS VUSB poison`。valid bit 0はCHRSTATUS、bit 1はVUSBの固定レジスタ読取り成功。poisonがFFFFFFFFならPMIC IOCTLが得られていない。これらは既存のPMIC状態であり、コネクタの5VやADC結果ではない。WACS2のタイムアウトは共有バスをpoisonし、CPU・タッチを含む他の利用者に影響し得る。

BQHRは `A04BQHR SNAP <id> 0 <result> <NTSTATUS> 0 0`。result 0はACPI IOCTL失敗、1は応答形式不正、2は記録不正、3は構造上STOPPED、4はその他の構造上有効な状態。FFFFFFFFはデバイスを開けず、FFFFFFFEはWindows側IOCTL取得失敗。result 3または4では1〜8行に128バイトを4個のlittle-endian 32bit値ずつ出力する。停止記録の構造検証は真正性・鮮度・ファームウェア／EL3からの排他引継ぎ・watchdog継続を証明しない。BQHRドライバはACPIのMOU0008にバインドし、同じデバイス上の燃料計機能ドライバと共存できない。I2Cのマップ、読取り、書込みは行わないが、MOU0008のPnPバインド変更自体は起こり得る。既存の燃料計ドライバを必要とする構成では競合を解決せずロードしないこと。

CPU UARTの `A04CP1` は既存媒体のCRC付き3重送信。USBとBQHRを先に実行してから開始する。UART収集、CPU値、タッチ、GPU、BQHRはそれぞれ別に成功判定する。USB・充電器の給電改善、実電圧、VCDT、実機署名信頼は未確認。実機の操作・書込みは主担当のみ。
