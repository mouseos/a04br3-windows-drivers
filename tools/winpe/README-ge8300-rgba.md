# GE8300 全画素診断媒体

`ge8300-render-rgba-manifest.json` はドライバ `c1b2f9e585b8cdb6c58da70ffe63d6da762f6dc1` の署名パッケージとESPを固定する。描画jobは10202faと同じで、09c630dの矩形化原因は未解決。今回の目的は未記録だった画素の位置と値を同一起動で収集すること。既定OFFの実験診断であり、実機での起動・署名受理・GPU実行・UART完走は未検証。

同じ起動のUARTに既存の64行マスク、render結果、fence・CCB位置の最後の同期済み読取り、partial/final指定、cleanupとFW停止結果を出す。fence値は逐次読取りであり同時点のスナップショットではない。fence、同期、cleanupが成功しstage=32の場合に限り、所有DMAの解放前に線形64×64 RGBA 16 KiBを64バイトごとのCRCと全体CRC付きで送出する。`tools/gpu/extract-render-pixels.py UART OUTPUT --offset START` は全256断片、各CRC、BEGINとENDの全体CRCを検査し、欠落・混在・破損・重複を拒否する。対象ログは起動範囲に切り出すこと。出力は画素の生データであり公開前に確認する。画面送信はGPU診断が戻った後に行い、800×1280で観測する。

成功statusは三角形の画素条件、fenceとcleanupはソフトウェア観測であり、FW shutdown成功でもハードDMA停止の独立証明にはならない。timeout時は未同期画素を出力せずDMAを保持する。停止未証明のreset・復元制約、既存の画面・復元候補、偽陽性回避の詳細は `README-ge8300-linear.md` の観測と復旧を参照する。以前の「観測分類のみの再投入はしない」という記述に対し、この媒体は画素全体を追加取得する別の観測である。

## 再生成

署名にはREADME-ge8300-linear.mdと同じARM WDK、４フラグ、固定コミットからのビルドを使う。公開証明書のSerializedCertと７ファイルはmanifestのSHA-256に固定し、秘密鍵を含めない。`SOURCE_WIM`、`TEMPLATE_ESP` は従来の画面収集実績のもの、`PACKAGE` は署名パッケージを展開したディレクトリ、`CERT` はSerializedCert、`OUT` は新規出力先とする。`wimlib-imagex`、`hivex`、`pyfatfs`、`fsck.vfat` が必要。

```sh
python3 tools/winpe/prepare-ge8300-rgba-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 tools/winpe/add-ge8300-rgba-cert.py --source "$OUT/boot.wim" --source-sha256 "$(sha256sum "$OUT/boot.wim" | cut -d' ' -f1)" --serialized-cert "$CERT" --output "$OUT/boot-trusted.wim"
python3 tools/winpe/verify-ge8300-base.py --base "$SOURCE_WIM" --candidate "$OUT/boot-trusted.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p=pathlib.Path(sys.argv[1]); w=json.loads((p/'boot-trusted.wim.json').read_text())
(p/'stage.json').write_text(json.dumps({'version':1,'token':secrets.token_hex(16),'wim_sha256':w['wim_sha256']})+'\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 80e78974e86c709f0961d4f80db5dcaeff3dade8bf2ab5c86b7102aad4035871 --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot-trusted.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp.img"
```

試験対象と既知復元候補は `check-ge8300-rgba-inputs.py` で４ファイルのハッシュを確認できる。WIM/ESP生成時刻等で再生成物のハッシュは変わり得るため、manifestは今回生成した固定成果物を指す。証明書のoffline信頼登録はWIMから再抽出して照合済み。実機側の署名ポリシーは未確認。
