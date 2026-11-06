# GE8300固定コピーのログ判定

`analyze-ge8300-copy.py` は既存UART収集ログの今回の起動だけを、明示した行頭の開始位置と終了位置で読み取り、JSONに保存する。端末のUART、reset、書込み装置を開かない。準備時点で合成ログ試験済みで、実機結果は未確認。

```sh
python3 tools/uart/analyze-ge8300-copy.py \
  --log "$LOG" --start "$START_OFFSET" --end "$END_OFFSET" --output "$REPORT" \
  --restore-boot "$RECOVERY_BOOT" --restore-boot-sha256 "$BOOT_SHA256" \
  --restore-esp "$RECOVERY_ESP" --restore-esp-sha256 "$ESP_SHA256"
```

ログ、出力、任意の復元イメージはいずれも実体が作業ルート内にあること。出力は既存ファイルを上書きしない。復元イメージはファイルと期待SHAを対で指定し、ハッシュ不一致なら失敗とする。終了コードは一致0、失敗1、不明2。

EBSとPOSTがそれぞれ1回であること、再起動の混入がないこと、power/start/fence、4 context、コピー結果、cleanup、forced idle、OFFの順序を厳密に照合。コピーは `STATUS=0 MATCH=400 GUARD=0 PHASE=4` と非ゼロUFO一致が必要。cleanupは `STATUS=0 FAILED=4 MASK=F`。完全な行でないものやUART混線、重複は失敗または不明。GPU側に記録されない保護領域、出力内容の全外部検証、DMA静止の証明ではない。

`A04VOL_LISTENER_READY` とそれに続く完全な `A04VOL_RELEASED` だけを解除応答受信として記録する。混線、欠落、失敗応答や再起動混入は解除成功としない。READY反復は送信保証ではない。GPUの直接MMIO UART出力は他のUART処理と衝突し得る。

復元画像の整合性と解除応答をまとめて判定するが、自動reset・書込みは実装していない。解除やFW cleanup、OFFだけではGPU DMA停止が証明されず、自動復元へ移行する根拠がないため。復元ゲートは常に閉じ、結果をJSONに明示する。現在動作中のWinPEに送信しない。
