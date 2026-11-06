# m1.11 bounceコピーサービス診断

既定OFFのWinPE専用実験。`A04_GE8300_VENDOR_COPY_DIAGNOSTIC=1` と `A04_GE8300_VENDOR_COPY_SERVICE=1` を有効にしてビルドする。起動後に専用source・destination・sharedの各4KiBへ固定GPU仮想アドレスを設定し、入力4096バイトをCPU側のexpectedとsource common bufferへコピーする。TQとUFO、SLC flush、GP fence後に入力・出力・shared・隣接ガードを照合し、一致時だけ結果を呼出し元へ返す。POST framebufferやWindows呼出し元バッファをGPUにはmapしない。

同一起動中の複数要求を直列化するが、CCBの折返しを実装せず空き不足で拒否する。現状のcontext初期化後は最大13要求。timeoutやデータ不一致後は資源を保持し次の要求を拒否。D0 exitは新規受付を閉じ、処理中要求を寿命ロックとWDF power-managed queueで排出する。surprise removalは新規受付を閉じるが、既に進行するMMIO・DMAや電源喪失、装置削除後の資源寿命は未証明。C:解除・FW fence・OFFはDMA停止を意味しない。Windows本体への導入は不可。

`tools/ci/build-ge8300-vendor-copy-service.ps1` に元firmware、REA抽出USC、未作成のrunner作業ディレクトリを渡す。ドライバ・診断EXEをARM32 WDK 8.1でビルドし自己署名ZIPを作る。証明書はビルドごとに変わる。署名受理は実機未確認。

`prepare-vendor-copy-service-wim.py` は0x714親とREADY反復listenerの固定媒体に署名ZIPを追加する。`--source-wim`、`--source-esp`、`--boot`、`--package`、`--package-sha256`、`--driver-commit`、`--output`を指定。実体・作業先をルート内に限定し空のoutputを使う。診断EXEは任意パターンを3回送ってコンソールに `VENDOR_COPY_PASS` 3回と `VENDOR_COPY_THREE_PASS` を出す。UART画面送信は自動実行しない。listenerやGPUのUART出力は衝突し得るため、欠落・混線した結果や解除応答を成功扱いしない。実機は未試験。

診断は実行後、`A04BR3ScreenUart` の共有オープンを最大30秒試し、送信busyも再試行する。結果は `A04CP1` の行CRC付きレコードを各3回送る。入力・出力CRC、要求番号、IOCTL結果、返却バイト数、不一致数と終端がすべて揃った場合のみ `tools/recovery/decode-ge8300-copy-uart.py` が成功とする。IOCTL成功はドライバ内のfence待ちと照合の成功を表すが、fence値の直接観測やDMA停止の証明ではない。listenerとGPU直接出力が混線した場合、CRC検証に通る完全な反復だけを採用し、欠落や矛盾は不明とする。取得ログは起動ごとに分ける。

診断停止IOCTLはcopy受付を閉じて実行中の要求と排他し、4 context cleanup、forced-idle、forced OFFを順に試みる。UARTの `A04GPU STOP` は各NTSTATUS、cleanup mask、FW queueとpower state、OFF応答後のsidekick/SLC/BIF/MMUの単回値を示す。失敗時は以降の段階を実行しない。単回レジスタ値とFW OFFはIRQ排出、RGXStop、外部fabricのDMA停止の証明ではない。メモリとadapterを保持し、D0 exitおよびunloadを許可しない。物理抜去との競合は未検証。診断用であり実機での安全なunloadの実装ではない。
