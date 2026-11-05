# LC709203F 更新ドライバのWinPE診断

`prepare-lc709203f-refresh.py` は残量39%を取得した既存WinPEのWIMと、署名済み `dcbb0d0` パッケージのSHA-256を検査し、そのコピー内の燃料計ドライバとステータスツールを置換する。WIM内のRoot・TrustedPublisherへ対象テスト証明書を追加し、全メンバーと証明書をWIMから抽出・照合する。元WIM、EFIや実機は変更しない。ベースWinPEにある既存のGE8300表示診断も維持する。

必要な入力は `windows-rt/lc709203f-9a95c80/boot.wim`（SHA-256 `99c947f61fe0d8142d66938c516d2317dde0481def30d620c9f90e5b5961efb7`）と、`port/xnu/out/windows-rt-lc709203f-dcbb0d0/` の `signed.zip` と `manifest.json`。元となるWindows回復媒体は利用者が用意する。wimlib-imagex、hivexsh、hivexget、OpenSSLが必要。パスに空白・引用符は使えない。

作成WIMは通常の `prepare-mma8652-esp.py` で、実機WinPEで使用したESP `63f7858d7ee3b78af4aeb5db854774552f2e9f12f67eb804b04fbddeed775f4c` と検証済みBCD `65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d` に組み合わせる。stage manifestは `version: 1`、ランダムな16バイトの16進 `token` と生成WIMの `wim_sha256` を指定する。

この媒体と更新ドライバの署名受理・PnP・残量読取りは実機未検証。以前の39%は旧ドライバと旧UEFIでの結果であり、新版の結果ではない。充電器とI2C2を共有する構成、搭載Windowsへのインストール、スリープ復帰も検証していない。媒体にはWindows本体を変更する手順を追加していない。
