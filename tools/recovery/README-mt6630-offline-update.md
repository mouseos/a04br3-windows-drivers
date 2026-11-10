# MT6630単独の保守更新と通常Windowsの保持

`run-mt6630-offline-update.py`は、署名済みMT6630 package用の検証済み
保守媒体を一度起動し、同じ起動のCRC付き結果を確認して通常ESPへ戻す。
対象はTAB-A04-BR3、serial `0C0U265083`のみ。

実行前に対象をAndroid Recoveryのrootへ切り替え、同じRecovery起動中に
`capture-windows-prebackup.py`でfresh backupを取得する。通常Windowsを
再起動した後の古いbackupは使用しない。媒体builderがそのmanifestと
最終署名packageを固定する。公開リポジトリへprivate backupや媒体を含めない。

```sh
python3 -B tools/recovery/run-mt6630-offline-update.py \
  --workspace "$PROJECT_ROOT" --media "$MEDIA_DIRECTORY" \
  --backup "$FRESH_CAPTURE_MANIFEST" --out "$NEW_OUTPUT_DIRECTORY" \
  --manifest-sha256 "$REVIEWED_MEDIA_MANIFEST_SHA256"
```

媒体は`manifest.json`に`esp_sha256`、16桁大文字hexの`token`、
`host_backup_manifest_sha256`を持ち、`update-esp.img`は256MiBとする。
callerは実行前にbuilder・source・署名package・更新対象がWiFiのみであることを
確認し、manifest SHA256を明示する。

controllerは同Recoveryのbackupを再検査し、RAM転送ファイルをhash確認後に
ESPを書き込む。NTFS/fuseblk、対象Windows/EFI領域のmountを検出したら停止する。
物理ESPのreadback、boot/GPTの書込み、更新サービスの登録、追加診断は行わない。

CRC付き報告にCOMMITTED、release exit0、update exit0が各一度ある場合だけ
UART resetを要求し、機種確認後Recoveryへ戻る。既知の通常ESPを復元して
通常Windows起動を要求し、それ以降の再起動・回収は行わない。
書込み・UARTの記録は指定作業ルート内へ保存する。

報告欠損では処理中の可能性があるためresetしない。DISM/release失敗では
通常Windowsへ進まない。新規出力先だけを受け付け、同じ更新を自動再実行しない。
復帰段階で失敗した場合は保存resultに基づいて復帰だけを行う。

COMMITTEDはオフラインservicing成功であり、新driverの選択・開始、接続、
認証、IP通信の成功とは区別する。通常起動要求もデスクトップ到達の証拠ではない。
