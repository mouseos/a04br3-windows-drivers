# TAB-A04-BR3 Windows ドライバ

TAB-A04-BR3のWindows RT 8.1 ARM32向けドライバをまとめるリポジトリです。ストレージ、入力、USBなどを機能別のディレクトリに置き、ドライバごとにリポジトリは分けません。UEFIの共通コードやWindowsのバイナリは含めません。

現在、WinREは表示できますが、本端末のeMMCとタッチはWindowsに認識されていません。このリポジトリに動作するドライバはまだありません。まず `drivers/storage/mt8167-msdc` で、ストレージ要求を検証するOS非依存コードとホストテストを実装しています。ハードウェア操作、PnP、割込み、DMA、StorPortへの接続、Windows 8.1 ARM WDKでのビルド、署名、WinREへの導入と実機試験は未実装です。

## ローカル検証

C11コンパイラとmakeで `make test` を実行します。Clangがある場合、`make arm-coff` はARM32 Windows用COFFオブジェクトの生成を確認します。これらはWindowsドライバのビルドやロード成功を意味しません。

Microsoftは `SdPortInitialize` の対象をWindows 10以降と記載しています。そのAPIをWindows RT 8.1で利用できると仮定せず、Windows 8.1 WDKと実際のOSにあるインターフェースを調査します。Linux上のClang/LLDによるARM PE生成は形式を確認済みですが、WDK 8.1のARMヘッダー・ライブラリはWindows Actionsへのインストールと存在を確認済みですが、実ドライバのWDKビルド、コード署名、Windows RT固有のロード条件は未確認です。必要に応じてこの同じリポジトリでWindows GitHub Actionsを利用します。

- [Microsoft: SdPortInitialize](https://learn.microsoft.com/en-us/previous-versions/mt715818%28v%3Dvs.85%29)
- [Microsoft: Windows 8.1のテスト署名](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing)
- UEFI開発元: https://github.com/mouseos/edk2-mtk （公開ブランチ `windows-rt/arm32`）
