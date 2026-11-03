# recoveryからの実験用ドライバ更新

`update-driver-ntfs.py` は同一INFのテスト署名済みブートドライバを、全Windowsイメージを書き直さずに更新する実験手順です。既存の正しいインストールとrootのAOSP recovery、端末で動く静的NTFS-3Gが必要です。指定のWindows領域位置、未マウント状態、転送ハッシュ、既存INFとの一致を確認し、変更前のSYSとCATを指定先へ保存してから、既存ファイルの内容を更新します。GPTや他領域は書きません。

```sh
python3 tools/recovery/update-driver-ntfs.py \
  --package /path/to/test-signed-package \
  --ntfs3g /path/to/static-aarch64-ntfs-3g \
  --backup-dir /path/to/new-backup-directory
```

この方法で置換したカタログがWindowsのカタログデータベースと整合することや、署名が受理されることは未検証です。失敗時のバックアップを保持し、起動結果を確認してください。正式なドライバインストールや署名を置き換える製品手順ではありません。
