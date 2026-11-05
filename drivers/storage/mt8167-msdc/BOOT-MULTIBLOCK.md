# 起動構成の複数ブロック読取り試作

`A04_BOOT_MULTIBLOCK_EXPERIMENT=1` は既定OFF。既存の `A04_BOOT_PASSIVE_EXPERIMENT` 一式を要求し、読取り専用の `A04_DMA_MULTIBLOCK_EXPERIMENT` と併用できない。WDK ビルド時は `.github/workflows/wdk81-probe.yml` の環境変数一式を指定する。OFF/ON の両方をビルドする手順を同 workflow に用意した。これは未検証の試作であり、実機への導入や本体起動の安全性を示さない。

最大8セクタの読取りだけ CMD18/CMD12 にまとめる。書込みは既存の GPT 範囲制限、単一セクタ CMD24、DAT0 と CMD13 の待機を維持する。共有 4 KiB バッファは割当て後に連続長・アラインメント・32ビット範囲を検査し、各 DMA 前に既存の MDL と `KeFlushIoBuffers` を使用する。失敗時は要求を失敗とし、部分結果を返さず再試行しない。転送エラーまたはバスリセット後の再開は、DMA とカードの回復手順が未確立のため拒否する。単一要求ゲートと隔離 SRB は既存構成に従う。

重大な制限: Storport の virtual miniport から実ハードウェアにアクセスする既存経路は未サポートであり、共通バッファの実際のキャッシュ属性と ARM DMA 同期の正当性は未証明。DMA 停止失敗時のハードウェア停止、停止中の surprise removal、worker の寿命、電源喪失後の再初期化も未解決。Stop/Restart のゲート獲得失敗は安全な PnP 完了を証明しない。物理経路では過去に DMA 整列違反で停止した。実装を本番用途やデータのある媒体で使用しないこと。

既存の3コア WinPE の 4 KiB × 32768 回、13.485 秒の対読一致は読取り専用・非起動の仮想バッファ構成の結果で、この起動試作の検証ではない。ホスト試験も実機のキャッシュ整合性や電源・PnP を検証しない。

参考: Linux v6.6 の [`mtk-sd.c`](https://github.com/torvalds/linux/blob/v6.6/drivers/mmc/host/mtk-sd.c) は DMA map/unmap と停止完了確認を行い、複数ブロックは CMD23 の有無に応じ停止コマンドを選ぶ。Linux の DMA API は Windows の同期保証を代替しない。

停止競合の保守的な試作では reset 時に実行中の SRB を先に完了せず、worker に
完了を委ねる。worker は DMA の停止を証明できなかった場合、SRB と要求ゲートを
保持したまま戻り、新しい読取りや書込みを受け付けない。これは Storport が所有する
extension と共通バッファの寿命を延ばす API ではなく、PnP 削除に対する安全性を
証明しない。現在の bounce は依然 Storport 所有で、独立した DMA adapter と
ドライバ所有バッファへの移行は未実装。起動拒否を維持し、実機に投入しない。

Windows の `IoGetDmaAdapter` は物理 PDO を用い、デバイスが使うアドレスは
`AllocateCommonBuffer` の logical address として取得する。CPU 物理アドレスを
そのまま DMA バスアドレスと仮定しない。PnP 開始・削除、DMA 停止不能時の
adapter と割当ての寿命を実装する前に、単なる contiguous pool への置換で
所有権が解決したと扱わない。
参照: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetdmaadapter
および https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer

boot/scratch の試作では各コントローラ MMIO を原子的な受付状態で保護する。
stop/removal が受付を閉じた後の新しい MMIO は行わない。DMA 中に受付が閉じた
場合、後から停止成功と見えても閉鎖後の読み値は実機の状態を証明しないため、
DMA 状態を保持し poison とする。受付閉鎖前に既に始まった MMIO への物理的な
バス消失は防げない。この試作も Storport extension の寿命を保証しない。
Microsoft の [HwStorAdapterControl](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_adapter_control)
では停止・再開は同期処理であり、ScsiStopAdapter/ScsiRestartAdapter は DIRQL と
InterruptLock の下で呼ばれる。失敗値の返却だけで削除を阻止できるとみなさない。

試作の tracked DMA が停止を証明できず保持される場合、当該転送の MDL も解放
しない。MDL の保持だけで Storport 所有バッファや adapter、worker、SRB の PnP
寿命が延びるわけではない。これらの実所有が実装されるまで起動を拒否する。

既定 OFF の `A04_OWNED_DMA_EXPERIMENT=1` は仮想 boot multiblock 試作に限り、
`StorPortGetDeviceObjects` の PDO に `IoGetDmaAdapter` を呼び、返された adapter の
`AllocateCommonBuffer` から DMA 用 logical address とバッファを取得する。所有者は
非ページ pool の別オブジェクトで PDO を参照し、32-bit の範囲と CPU・DMA 両側の
整列を検査する。起動は従来どおり拒否し、この拒否時にはまだ I/O が開始していない
所有領域を解放する。所有 adapter が使えるか、ARM のキャッシュ処理が適切か、
Windows RT 実機では未検証。通常構成は従来の確保経路を使う。

worker は依然 Storport の extension と callback に依存し、最後の削除 callback と
競合すると独立した bounce だけでは保護できない。`HwFreeAdapterResources` が
存在しても、その callback から進行中 worker を排出する設計と、停止不能 DMA に対する
adapter mapping の寿命は実装されていない。起動ゲートを維持し、実機に投入しない。
`tools/ci/build-msdc-multiread-suite.ps1` の `owned-boot-compile-only` がこの構成を
別々にビルド・自己署名する。証明書の信頼と署名の存在は別である。

所有 DMA 試作は仮想 miniport の `HwFreeAdapterResources` で worker の rundown を
開始し、queue 前に取得した rundown 参照が worker の Storport・extension・SRB への
最後のアクセス後に返るまで PASSIVE_LEVEL で待つ。待機後も DMA 停止、保留 SRB、
MDL のいずれかが残る場合は adapter・PDO・共通領域を解放しない。これは停止不能な
ハードウェアのバスマッピングが PnP 削除後も有効であることや、保留 SRB の Storport
による扱いを保証しない。最終 callback と StartIo/worker の実機上の順序、待機中の
ロック依存、ARM DMA 同期も未検証で、起動拒否は維持する。

Microsoft の [HwStorWorkItem](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_workitem)
は callback 起動時に Storport がロックを取得しないと明記する。ただし最終解放 callback
の間に worker の他の Storport API が内部で待つ可能性や、正常に queue 済みの作業と
削除の全順序までは同記述から証明できない。`RequestComplete` 後の SRB は Storport
所有であり、miniport は再参照できない。
[common-buffer DMA](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-common-buffer-bus-master-dma)
では要求バイト数の `BYTES_TO_PAGES` が `IoGetDmaAdapter` の map register 数を
超えてはならず、PnP stop/remove 時に `FreeCommonBuffer` するよう規定する。従って
停止不能 DMA のためバッファを保持し続ける現在の quarantine は PnP 完了の解決では
なく、削除後のバスマッピング保持の根拠にもならない。ハードウェア停止と要求排出を
正常に完了できる別経路が必要。実験ゲートを解除しない。

端末に同梱された Linux 4.4 の `drivers/mmc/host/mediatek/mt8167/sd.c` は
DMA STOP 後に `MSDC_DMA_CFG_STS` の消失を待つ。停止できなければ FIFO を消去しない
（`msdc_clr_fifo`）。同じソースの `msdc_reset_hw` は host reset、FIFO clear、
interrupt clear の順であり、上流系 `drivers/mmc/host/mtk-sd.c` も DMA 停止を
待ってからエラー時に host reset する。これは停止不能 DMA に reset をかければ必ず
止まるという証拠ではない。

所有 DMA の boot 試作に限り、読取りエラー後、受付が閉じていない間に bounded な
DMA STOP を再要求し、inactive 確認後に host reset、再度 inactive 確認、FIFO と
interrupt clear、最後の inactive 確認を行う。失敗または途中の所有権喪失では
DMA 状態・SRB・領域を保持する。成功はその要求をエラー完了できる材料に限り、
カード再開や書込み許可とはせず、失敗状態を維持する。書込みにこの回復を適用せず、
書込み結果を推測して再試行もしない。実機で DMA 停止を保証するものではない。

[HwStorResetBus](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_reset_bus)
は StartIo ロックを保持した DISPATCH_LEVEL の callback 中に対象 SRB を完了する
責任を miniport に課す。現行試作の停止不能時の保留はこの契約との整合が未解決で、
PnP 削除後の DMA mapping 保持も未保証。実機投入の根拠ではなく起動拒否を維持する。
