# GE8300/MT8167表示ドライバの通常Windows適用媒体

`build-ge8300-installed-update-media.py`は、検証済みmaintenance WinPEを基点に、署名済みproviderとdisplayの2パッケージだけをDISMで追加する媒体を作る。媒体生成は適用・通常Windows動作の成功を意味しない。

引数は`--workspace`、`--base`、`--provider`、`--display`、`--out`、`--tools`、`--source-commit`。前6項目はパス、最後はパッケージをビルドした完全な40桁commit。`base`には固定hashのmaintenance-staged.wim/ESP、`tools`にはwimtools/とpython-fat/を用意する。入力・出力はworkspace内に置く。INF版は旧provider 0.0.1.2/旧display 0.0.1.0より新しくなければ拒否する。

端末側は既存のMSDCパッケージ確認・Windows領域の識別・journal・DISM・領域解放・CRC付きUART結果送信を再利用する。CPU負荷・USB試験・GPU診断描画は実行しない。更新前のSYSTEM/SOFTWAREをjournalディレクトリに保存し、providerの開発証明書をオフラインSOFTWAREのRoot/TrustedPublisherへ追加する。これは全領域バックアップではない。DISM途中失敗を成功扱いせず、journalとレポートを保持する。

実行後のレポートで`UPDATE_STATUS=COMMITTED`、`UPDATE_RELEASE_EXIT=0`、`UPDATE_EXIT=0`が同じbootで確認できるまでresetしない。確認後、通常ESPと対応UEFIへ切り替えてPnP・実描画を検証する。適用時の成功はDISM追加と領域解放までで、3D描画やsleep/resumeの成功ではない。カメラは不要。
