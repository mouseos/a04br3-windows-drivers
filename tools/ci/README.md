# Windows ActionsのSSH介入

`A04_PASSIVE_IO=1` は、Storportワークアイテムの読取り専用WinPE診断です。書込み機能とブート用パッケージとの組合せはビルド時に拒否します。要求取消し・アダプター削除時の安全性と実機動作は未検証です。通常OSに登録しないでください。

`Windows 8.1 WDK availability` を `ssh_debug=true` で手動実行すると、SDK/WDKの準備が成功または失敗した後、同じWindowsランナーを最大180分保持します。ジョブ全体の上限は210分です。終了時にランナーと変更内容は失われるため、必要な修正はリポジトリへ反映しpushしてください。WDKのビルドが自動で成功することを意味しません。

GitHub CLIでリポジトリに認証し、実行IDを確認した上で `python3 tools/ci/connect-runner.py <実行ID>` を実行します。接続に使う秘密鍵は開発機のSSH agentまたは通常のSSH設定に置きます。リポジトリシークレット `A04BR3_RUNNER_SSH_PUBLIC_KEY` に公開鍵のみを登録する必要があります。ワークフロー起動者のGitHub公開鍵も許可されます。非対話コマンドはIDの後に指定できます。手動終了する場合、ランナーのチェックアウトに `end-ssh` ファイルを作成します。

接続情報とランナーの一時ホスト公開鍵は、その実行のGitHub Actions artifactから取得します。中継サービスのホスト鍵はリポジトリで固定し、ランナー自身のホスト鍵もartifactに照合します。公開中継 `ssh-j.com` を使うため、中継の可用性に依存し、接続先を外部に公開します。ランナーは公開鍵認証に限定します。公開リポジトリのActionsには機密データを置かず、信頼できる起動者・鍵のみを許可してください。`ssh_debug=false` が既定です。

大きなファイルは標準入力ではなく、同じホスト鍵照合を行う `python3 tools/ci/copy-runner.py RUN_ID upload LOCAL D:/path/file` または `download` でSFTP転送できます。ダウンロードは一時ファイルへ保存後に名前を確定します。転送元・転送先のハッシュは別途照合し、権利を確認して非公開の一時領域だけで扱ってください。

`Windows runner SSH smoke test` は同様の接続を6分間検証する専用ワークフローです。
