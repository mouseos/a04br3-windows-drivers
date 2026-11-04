# GE8300固定描画：WinPE試験候補

`ge8300-render-asymmetric-manifest.json` は実機復旧待ちの試験候補と入力を固定する。候補ESP自体の実機起動・描画は未実施。４ CPU WinPE全画面試験の成功WIMと同じSYSTEM、Start=0のTOPRGU、画面送信EXEを保持し、３点のハッシュとサービス設定を照合する。過去の試験ではCPU1所有引継ぎ、watchdog給餌、800×600・2500片の画面の全CRC一致を実測した。これは09c630dの描画、タイムアウト後のDMA停止、長期安定を証明しない。

WIMのstartnetは `wpeinit` の後、GPU firmwareをRAMディスクへコピーし、09c630d INFを`drvload`で一回だけ要求する。続いてPnPとstatusを表示し、戻った場合だけ既存の画面送信EXEを `-f` で一回実行する。描画はdrvloadの終了コードではなく、ドライバのD0 entry内で起動される。PnP未一致、署名拒否、firmware準備失敗、初期化失敗、実行中の停止はいずれも描画成功ではない。`drvload` が戻らなければ後段の画面送信には進まない。ディスク書込み、再試行、サービスのアンロードは行わない。

WIM派生物の `boot-trusted.wim` は、09c630dの公開テスト証明書をオフラインSOFTWARE hiveのRootとTrustedPublisherへ登録し、両BlobをWIMから再抽出して照合済み。テスト証明書を信頼する実験専用イメージである。`.cer` を配置するだけの `boot.wim` は登録しない中間成果物。BCDのtestsigning設定、Windows RTの署名ポリシーとドライバ受理は未検証である。

## 再生成

既存の単一イメージ・boot index 1の画面収集WIM（SHA-256 `f3808bb5fe590af7e21549312adf2f903dd5e5449b1d357967c270f3ae1a7648`）と、09c630dの同一署名パッケージ７ファイルを用意する。WIM編集用の入力パスに空白や引用符を含めない。出力は新規の通常ファイルとし、ブロックデバイスを指定しない。`wimlib-imagex` と `pyfatfs`、`fsck.vfat` が必要。

```sh
python3 tools/winpe/prepare-ge8300-asymmetric-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 tools/winpe/add-ge8300-asymmetric-cert.py --source "$OUT/boot.wim" --source-sha256 "$(sha256sum "$OUT/boot.wim" | cut -d' ' -f1)" --serialized-cert "$SERIALIZED_CERT" --output "$OUT/boot-trusted.wim"
python3 tools/winpe/verify-ge8300-base.py --base "$SOURCE_WIM" --candidate "$OUT/boot-trusted.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p = pathlib.Path(sys.argv[1]); w = json.loads((p / 'boot-trusted.wim.json').read_text())
(p / 'stage.json').write_text(json.dumps({'version': 1, 'token': secrets.token_hex(16), 'wim_sha256': w['wim_sha256']}) + '\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 80e78974e86c709f0961d4f80db5dcaeff3dade8bf2ab5c86b7102aad4035871 --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot-trusted.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp-trusted.img"
```

`SERIALIZED_CERT` は09c630dパッケージの `.cer` をWindows上で `[System.Security.Cryptography.X509Certificates.X509Certificate2]` として読み込み、`Export([System.Security.Cryptography.X509Certificates.X509ContentType]::SerializedCert)` で作成する。公開証明書のみで秘密鍵は不要。スクリプトは固定SHA-256を検査する。

WIM全データ検証、追加ファイル抽出・元ファイル照合、boot index、BCD等の保持、FATのWIM読戻し・fsckを行う。ツールバージョンや作成時刻で派生物のバイト列は変わり得るため、今回の固定成果物はmanifestのハッシュで区別する。入力の画面試験ではboot index修正版が実機起動し、全画面のCRC一致を確認した。GPU診断を組み込んだ派生WIMの起動を保証するものではない。

## 観測と復旧

試験前に連続UART収集を維持し、**今回の起動前**のログオフセットを記録する。試験後、次の起動が混ざる前の終端オフセットを固定して `python3 tools/uart/analyze-ge8300-render.py --log "$UART_LOG" --offset "$START" --end "$END"` を使う。単一EBSの後に重複のない64行の前景マスクと完全なrender行があり、bugcheckなし・STATUS=0・STAGE=32・全4096画素の色と対角線以外の座標が一致すると `spatial_match_morton_assumed` と判定する。64×64のMorton配置を仮定した結果であり、PBEの実機上の配置、context switch、GPU安全停止を証明しない。scissorを `0<=x<49, 0<=y<64` に変更した非対称パターンなので、x/y転置と左右反転は同じ画素数でも区別できる。解析JSONには64行の前景マスクと最初の不一致座標を含む。複数起動・複数行・不完全行・無出力は不明。`BOUNDED=0` はcontext switchの証拠ではない。画面は既存 `tools/uart/decode-windows-screen.py` で全断片とCRCを検証する。画面送信失敗やUART競合の可能性は残る。デコードできない時のみ画面監視の別手段を使う。

試験前に `check-ge8300-asymmetric-inputs.py --test-boot "$TEST_BOOT" --test-esp "$TEST_ESP" --restore-boot "$RESTORE_BOOT" --restore-esp "$RESTORE_ESP"` で４入力の固定ハッシュを検証する。復元候補はboot `7d03e814b3f156c4a03ef48c48ededd130916c5aa78df118d425f6ef40ea2ee3`、ESP `c2c1d2bfcf37b8422e9db9429b70fa98ffb7f995a6d24c8a5de3804d9b007176`。過去の全画面試験の入力記録と復元コマンド成功記録に一致する。続くUARTにはA04RGU給餌とMSDC CDBがあるが、複数の再起動が間に入り、ログだけでは復元直後の正確なboot・ESP組合せを確定できない。正しい対象・復旧方法・外部reset排他を主担当が確認する。既存 `run-display-winpe-test.sh` は終了時に自動reset・書戻しを試みるため、GPU DMAが停止した証拠のないこの試験にそのまま適用しない。watchdog期限やUART reset要求、fastboot復帰だけではGPU DMA停止・メモリ整合性は証明できない。タイムアウト、送信不明、cleanup失敗ではドライバはDMAを保持し、再送・アンロードを避ける。完全な電源断またはGPUを含む確実なハードウェアリセットと、復旧後の所有状態を主担当が確認してから復元する。未確認のまま成功扱い・再起動ループ・自動書戻しを行わない。通常の書込み後の実機読戻しは禁止。


`observe-ge8300-render.py --log "$UART_LOG" --offset "$START" --output-dir "$OUT_CAPTURE" --timeout 1400 --expected-size 800x600` は既存収集ログの対象範囲だけを保存し、画面全片・CRCを待って復元し、render結果を別途解析する。画面欠落・タイムアウト・不明な結果でもresetや書戻しは行わない。GPU診断が返らない場合、画面送信は始まらず期限まで不明となる。

UART行が欠落・混線・重複した場合や、旧751df54の集計行しかない場合は不明とする。出力は背景・前景の分類マスクであり、生BOやホストアドレスを公開しない。画面のCRC完了はGPU描画成功の代替ではない。

同じMesa由来USC・VDM・PPPを使い、元の64×64 scissorをMesaのIPF packingに従い49×64に派生させる。既存の２捕捉とMesa XML・16／32画素のregion clip境界を `tools/gpu/check-asymmetric-scissor.py` で照合した。非対称版自体のMesa再キャプチャとGPU実行は行っていない。Morton配置と実機命令意味・PBE書込み範囲は未証明。境界 `x+y=63` の該当41画素は前景または背景を許容し、それ以外の画素は位置と色を照合する。
