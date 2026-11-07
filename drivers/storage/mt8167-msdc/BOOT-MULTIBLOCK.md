# 起動構成の複数ブロック読取り試作

この文書はDMA試作の記録。現行同期PIOとは別経路である。現在の選択条件・開始拒否・過去実測との対応は [READ-PATH-STATUS.md](READ-PATH-STATUS.md) を参照。

`A04_BOOT_MULTIBLOCK_EXPERIMENT=1` は既定OFF。既存の `A04_BOOT_PASSIVE_EXPERIMENT` 一式を要求し、読取り専用の `A04_DMA_MULTIBLOCK_EXPERIMENT` と併用できない。WDK ビルド時は `.github/workflows/wdk81-probe.yml` の環境変数一式を指定する。OFF/ON の両方をビルドする手順を同 workflow に用意した。これは未検証の試作であり、実機への導入や本体起動の安全性を示さない。

最大8セクタの読取りだけ CMD18/CMD12 にまとめる。書込みは既存の GPT 範囲制限、単一セクタ CMD24、DAT0 と CMD13 の待機を維持する。共有 4 KiB バッファは割当て後に連続長・アラインメント・32ビット範囲を検査し、各 DMA 前に既存の MDL と `KeFlushIoBuffers` を使用する。失敗時は要求を失敗とし、部分結果を返さず再試行しない。転送エラーまたはバスリセット後の再開は、DMA とカードの回復手順が未確立のため拒否する。単一要求ゲートと隔離 SRB は既存構成に従う。

重大な制限: Storport の virtual miniport から実ハードウェアにアクセスする既存経路は未サポートであり、共通バッファの実際のキャッシュ属性と ARM DMA 同期の正当性は未証明。DMA 停止失敗時のハードウェア停止、停止中の surprise removal、worker の寿命、電源喪失後の再初期化も未解決。Stop/Restart のゲート獲得失敗は安全な PnP 完了を証明しない。物理経路では過去に DMA 整列違反で停止した。実装を本番用途やデータのある媒体で使用しないこと。

既存の3コア WinPE の 4 KiB × 32768 回、13.485 秒の対読一致は読取り専用・非起動の仮想バッファ構成の結果で、この起動試作の検証ではない。ホスト試験も実機のキャッシュ整合性や電源・PnP を検証しない。

参考: Linux v6.6 の [`mtk-sd.c`](https://github.com/torvalds/linux/blob/v6.6/drivers/mmc/host/mtk-sd.c) は DMA map/unmap と停止完了確認を行い、複数ブロックは CMD23 の有無に応じ停止コマンドを選ぶ。Linux の DMA API は Windows の同期保証を代替しない。
