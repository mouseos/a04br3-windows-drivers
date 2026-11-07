# eMMC書込みのまとめ処理

既定OFFの `A04_DMA_WRITE_MULTIBLOCK_EXPERIMENT=1` は、同一SCSI要求内の連続した最大8セクタをCMD25で転送する。1セクタは従来どおりCMD24。8セクタではCMD24/CMD13の8組をCMD25/CMD12/CMD13の1組にし、DMA用MDLの確保・キャッシュ同期・解放も8回から1回にする。実機での速度向上・書込み内容の検証は未実施。通常OS・WinPEへの適用は主担当で行う。

## 根拠と完了条件

Benesse公開ソース `CT3_GPL_LGPL_Src/kernel-4.4/drivers/mmc/host/mediatek/mt8167/` の `sd.c` を参照し、独立実装した。

- `msdc_command_start` はCMD25にmulti-block dtype=2とwrite bitを設定する。
- `msdc_dma_config` のbasic DMAは総転送長・LASTBUF・MODE=0を設定する。
- `msdc_if_send_stop` と `msdc_do_request` のstop経路は、CMD23/AutoCMD12を使わない複数ブロック転送を明示CMD12で終了する。
- `msdc_dma_stop` はSTOP後にDMA_CFG_STS=0を確認する。
- `msdc_reg.h` ではINT 0x2040はDXFER_DONEとDMAQ_EMPTY。XFER_COMPLは0x1000。DMA_CFG=4はSTS=0を意味するが、これだけでは転送成功ではない。

既存のXFER_DONE確認、CRC/timeout/bus error優先、STOP後DMA非稼働確認、CMD12のR1b応答・DAT0 high、書込み後CMD13のREADY_FOR_DATA・transfer stateを維持する。期限・poll上限も維持する。失敗時はSRBを成功にせず、再試行しない。SRB全範囲のGPTガードは分割前に適用する。異なるSRBを結合しない。

INTのW1Cは読み取った対象ビットだけに適用され、コマンド応答のackではデータ完了を消さない。既存Windows HwInterruptはackをしない。今回、不要な固定waitやINT ack raceを立証しておらず、その仮説を理由に完了条件・クロック・レジスタ値を変更していない。

## 有効化

既存のguarded isolated passive boot設定に、以下を追加する。既存multireadビルドだけでは書込み方式は変わらない。

```powershell
$env:A04_BOOT_MULTIBLOCK_EXPERIMENT = '1'
$env:A04_DMA_WRITE_MULTIBLOCK_EXPERIMENT = '1'
$env:A04_DMA_POLL_TRACE = '0'
```

非bootの限定scratch試験では `A04_BOOT_MULTIBLOCK_EXPERIMENT` の代わりに既存の `A04_SCRATCH_MULTIBLOCK_EXPERIMENT=1` を使用する。両者とも既存の必須設定と `A04_ENABLE_WRITES=1` が必要。4KiBバッファとworker/DMA寿命保護のない構成はコンパイル／ビルド設定チェックで拒否する。PS1の入力は `tools/ci/build-wdk81-probe.ps1`。新しい署名済みINF/SYS/CATを一組でビルド・署名して媒体へ反映する必要があり、ソース更新だけでは既存WinPEは変わらない。

## 実ハード名

実PNMとMIDを使い、別型番に推定置換しない。現行ソースf55a48bには、起動時の7バイトMID+PNM引継ぎからSCSI INQUIRYへ `SAMSUNG` と実PNMを返す実装が既にあり、本ブランチも継承する。ハードコードされたAJTD4Rの応答ではない。引継ぎ無効時はUNKNOWN/eMMC Deviceとし、偽の型番を返さない。CID serialを引継ぎ・公開しない。
