# GE8300固定描画：WinPE試験候補

`ge8300-render-751df54-manifest.json` は今回作成した候補の入力・出力を固定する。実機書込み、ロード、描画、watchdog復帰はいずれも未実施。特に **この候補をそのまま実機に投入しない**。既存雛形は画面転送TOPRGUのStart=0を含み、４ CPU・UART・watchdog所有の引継ぎは未証明。751df54のPBEアクセス範囲とGPU停止も未証明である。回復後、主担当が復旧経路と安全条件を確定する必要がある。

WIMのstartnetは `wpeinit` の後、GPU firmwareをRAMディスクへコピーし、751df54 INFを`drvload`で一回だけ要求する。続いてPnPとstatusを表示し、戻った場合だけ既存の画面送信EXEを `-f` で一回実行する。描画はdrvloadの終了コードではなく、ドライバのD0 entry内で起動される。PnP未一致、署名拒否、firmware準備失敗、初期化失敗、実行中の停止はいずれも描画成功ではない。`drvload` が戻らなければ後段の画面送信には進まない。ディスク書込み、再試行、サービスのアンロードは行わない。

テンプレートWIMにはこのGPUパッケージの新しい自己署名証明書は信頼登録されていない。`.cer` をWIM内に含めるだけでは登録・読み込み許可にならず、testsigningのBCDも受理を保証しない。署名済みファイルの一致と暗号学的検証はホストで行ったが、Windows RTのロードは未検証。信頼済み証明書のオフライン組込みと署名ポリシーを主担当が確定するまで、試験可能と断定できない。

## 再生成

既存の単一イメージ・boot index 1の画面収集WIM（SHA-256 `a51a478308832713ce877af51a4fecc69acbce40c680bbe08d41e4cc453386d8`）と、751df54の同一署名パッケージ７ファイルを用意する。WIM編集用の入力パスに空白や引用符を含めない。出力は新規の通常ファイルとし、ブロックデバイスを指定しない。`wimlib-imagex` と `pyfatfs`、`fsck.vfat` が必要。

```sh
python3 tools/winpe/prepare-ge8300-render-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p = pathlib.Path(sys.argv[1]); w = json.loads((p / 'boot.wim.json').read_text())
(p / 'stage.json').write_text(json.dumps({'version': 1, 'token': secrets.token_hex(16), 'wim_sha256': w['wim_sha256']}) + '\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 ed92f50a7ba5464226e9781be6613135ed02bada702d3eb23300d85b902bd26e --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp.img"
```

WIM全データ検証、追加ファイル抽出・元ファイル照合、boot index、BCD等の保持、FATのWIM読戻し・fsckを行う。ツールバージョンや作成時刻で派生物のバイト列は変わり得るため、今回の固定成果物はmanifestのハッシュで区別する。既存テンプレートのboot index修正版も実機未検証であり、スクリプト引数名の `verified` はその証明ではない。

## 観測と復旧

試験前に連続UART収集を維持し、**今回の起動前**のログオフセットを記録する。試験後、次の起動が混ざる前の終端オフセットを固定して `python3 tools/uart/analyze-ge8300-render.py --log "$UART_LOG" --offset "$START" --end "$END"` を使う。単一EBS・単一完全なrender行・bugcheckなし・STATUS=0・STAGE=32・指定ヒストグラムと全4096画素の一致を満たしても `histogram_match_only` であり、位置、向き、context switch、GPU安全停止を証明しない。複数起動・複数行・不完全行・無出力は不明。`BOUNDED=0` はcontext switchの証拠ではない。画面は既存 `tools/uart/decode-windows-screen.py` で全断片とCRCを検証する。画面送信失敗やUART競合の可能性は残る。デコードできない時のみ画面監視の別手段を使う。

試験前に同一端末の復元bootとESPを固定ハッシュで準備し、正しい対象・復旧方法・外部reset排他を主担当が確認する。現時点ではその復元ペアを特定できていない。既存 `run-display-winpe-test.sh` は終了時に自動reset・書戻しを試みるため、GPU DMAが停止した証拠のないこの試験にそのまま適用しない。watchdog期限やUART reset要求、fastboot復帰だけではGPU DMA停止・メモリ整合性は証明できない。タイムアウト、送信不明、cleanup失敗ではドライバはDMAを保持し、再送・アンロードを避ける。完全な電源断またはGPUを含む確実なハードウェアリセットと、復旧後の所有状態を主担当が確認してから復元する。未確認のまま成功扱い・再起動ループ・自動書戻しを行わない。通常の書込み後の実機読戻しは禁止。
