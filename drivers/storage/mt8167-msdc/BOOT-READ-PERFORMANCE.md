# 起動 eMMC の複数ブロック読取り：統合条件

対象は ARM32 の Windows RT 8.1。CPU 数を増やす試験は前提にしない。
この文書の調査基点は `88713749444e37c2459178be4bac3fe56b103684`。
GPU や共有親の試作に依存しない。

## 実測と現行経路

ユーザー申告と `MULTIREAD-VALIDATION.md` の記録では、同一署名済み計時 EXE、
同一の３コア WinPE で、4 KiB × 32768 回のペア照合は単一ブロック 29.687 秒、
複数ブロック 13.485 秒だった。比は約 2.20 倍。これは `5ec0aaa` の読取り専用
仮想バッファ・DMA 診断の結果で、書込みを含む OS 起動経路や外部正解との比較を
証明しない。署名のない EXE は起動できなかったとの申告がある。

本体起動実績のある構成は `README.md` のとおり、テスト署名した
`A04_BOOT_EXPERIMENT` と `A04_VIRTUAL_BOUNCE_EXPERIMENT` を使う。
PIO は GPT や EXT_CSD の初期観測に使うが、ディスク I/O は
`A04_DMA_DISK_PROBE` の単一セクタ DMA である。物理 PIO 専用の
`A04_SYNC_PIO_MULTIREAD` は別構成で、起動可能な書込みドライバではない。
実機に現在インストールされている SYS のハッシュはこのソースだけからは断定しない。

## 利用できる実装と安全上の差

- `5ec0aaa` の `a04_msdc_dma_read_blocks_timed` は最大８セクタの CMD18、
  DMA 停止確認、CMD12 と DAT0 待機を行う。現行 `dma.c` にも残る。
- `A04_BOOT_MULTIBLOCK_EXPERIMENT` は現行の GPT 書込み範囲検査と単一セクタ
  書込みを残し、読取りを８セクタ単位にする。ただし `A04_BOOT_PASSIVE_EXPERIMENT`、
  isolated SRB、非同期 worker を要し、`HwInitialize` は明示的に失敗する。
  この構成はビルド可能だが本体起動の実績はない。
- worker が走行中に reset が SRB を完了し得る。isolated SRB は元 SRB の参照を
  分離するが、Storport が所有する extension と DMA 領域の解放を阻止しない。
  停止を失敗させるだけでは PnP による資源寿命延長を保証できない。
- DMA 停止 timeout では物理デバイスがバッファに書き続け得る。単一セクタ経路へ
  自動で切り替えると新しい要求や書込みと競合するため、停止・カード状態の回復を
  実証するまで failover はできない。既存試作はエラーを返して再試行しない。
- 既存バウンスバッファは `StorPortGetUncachedExtension` で確保し、MDL と
  `KeFlushIoBuffers` を DMA 前に使用する。しかし非キャッシュとされる領域に
  明示的同期が必要だった実機事象の原因と、完了・異常時を含む ARM32 DMA の
  一貫性は未確定。物理 Storport 経路では E6/1F/04 整列違反を観測している。
  現行の仮想 miniport が物理コントローラを直接使用する点も適合性の課題である。
- 書込み中の電源断・キャンセル後は結果が不定であり、再試行してはならない。
  PIO と DMA の併用も controller、interrupt、カード状態の同一所有と直列化を
  必要とする。読み取り専用実測を根拠に、書込みありの起動ドライバへ切替えない。

`HwInitialize` の拒否、既定 OFF、既存書込み経路を維持する。未検証の最適化を
起動ボリュームに導入できるものとして配布しない。

## 再現可能なオフラインビルド

Windows SDK/WDK 8.1 ARM32 と clang/LLD を備えた隔離ディレクトリで、固定コミットの
ソースから次を実行する。スクリプトは readonly、scratch、boot、sync PIO の４種を
別々にビルドし、SYS・CAT と対象 EXE に自己署名してハッシュを記録する。

```powershell
./tools/ci/build-msdc-multiread-suite.ps1 -OutputRoot C:\a04-msdc-suite -SourceCommit 88713749444e37c2459178be4bac3fe56b103684
```

scratch の LBA 0 は GPT ガードにより拒否されるコンパイル専用値。boot と sync PIO は
初期化拒否のコンパイル専用。証明書は自己署名で、署名存在と Windows RT 上の
信頼は別。EXE にも署名が必要。別の実行中ジョブの checkout は使用しない。

## 次の実装・検証条件

単一／複数読み取りの経路を同じ controller 所有権と要求直列化の下に置き、停止時は
新規要求を閉じ、実行中の処理と DMA を排出してから Storport のメモリを解放する。
停止不能時は OS が資源を解放しないことを Windows 8.1 の契約で実証するか、寿命を
管理できる方式へ移行する。CMD18 の失敗後は DMA 停止、CMD12、DAT0、CMD13 と
カード識別・状態を確認し、これらが成立しない場合は書込みを含め再開しない。
競合要求、異常、停止と書込み境界をホストで検証し、別の復元可能な WinPE 環境で
既知データとの照合、スクラッチ限定の書込みと復元、署名・起動・復帰を主担当が
確認する。実機結果と OFF 版の両方を記録するまで性能改善の起動実績としない。

参考：Microsoft の [仮想 miniport](https://learn.microsoft.com/en-us/windows-hardware/drivers/storage/overview-of-storage-virtual-miniport-drivers)、
[KeFlushIoBuffers](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keflushiobuffers)、
[Linux MTK MSDC](https://github.com/torvalds/linux/blob/v6.6/drivers/mmc/host/mtk-sd.c)。
