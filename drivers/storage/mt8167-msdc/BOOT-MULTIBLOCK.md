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
