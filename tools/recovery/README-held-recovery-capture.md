# run234 の移行失敗後の回収

`resume-held-recovery-capture.py` は、run234 の既存 terminal hold と、Recovery 移行前に失敗した collection を保存したまま、既に起動している Recovery から actual235 を取得する専用入口です。汎用 retry ではありません。

- 元 controller は seal の SHA で確認してから読み込み、seal、実行 latch、terminal proof、保存 UART を既存検証関数で確認します。
- 旧 collection に capture 準備以降のファイルがあれば拒否します。capture / diagnostics 出力が存在する場合も拒否します。
- `--validate-only` は端末にアクセスしません。実行には対象 serial の Recovery と root が必要です。
- 実行結果は新規 `collection-recovery-ready/` へ保存します。このディレクトリが既にあれば再実行しません。旧 result / collection / seal は変更しません。
- reset、reboot、flash、インストーラー、Windows 診断クライアントは実行しません。既存 capture / collector による読み取り専用 NTFS マウント、回収、アンマウントを行います。Recovery の `/tmp` に NTFS ヘルパーを転送します。
- 実機上の機能成功は判定しません。回収後の主担当レビューが必要です。

指定する引数は `--workspace ROOT --seal SEALED_RUN234_JSON --seal-sha256 SHA256` です。元 controller / seal を変更せず、このスクリプト自体の commit と SHA も実行記録へ残してください。
