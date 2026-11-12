# 更新開始前の拒否後に回収を再開する

`capture-after-clean-rejection.py` は、WinPEのCRC付き報告が `BASELINE_REJECTED_NO_CHANGE` / release 0 / exit 6だった実行だけを対象にする。元sealとruntime resultをSHAで固定し、元の保存UARTを固定decoderで再検証する。旧report、seal、result、実行latchを変更しない。

対象serialのfastbootまたはRecoveryが現れるまで、指定時間だけ待機する。fastbootのproductを確認してRecoveryへ移り、rootを確認してから、新しいcapture・GPU補足・USB1121固定EXE witnessを一度だけ取得する。Windows領域、boot、GPTへの書込みや更新再試行は行わない。Recoveryの `/tmp` には既存NTFS helperを転送する。

`--workspace ROOT --seal FILE --seal-sha256 SHA --runtime DIR --runtime-sha256 SHA --output NEW_DIR` を明示する。`--validate-only` は端末へアクセスしない。`--wait-seconds` は最大19800秒、pollは5〜60秒。新しい回収先が既にあれば拒否する。待機中に `NEW_DIR/cancel` を作れば、回収開始前に終了する。回収中の中断後は残った証拠とmount状態を確認し、自動で再実行しない。

待機開始時のtool SHAを保存し、端末復帰後にも照合する。一時的なADB観測timeoutは待機期限内で再観測する。期限切れは新captureの成功ではない。`new-capture-and-supplements-ready` は回収完了だけを表し、native inventory、GPU描画やUSB給電の成功を意味しない。
