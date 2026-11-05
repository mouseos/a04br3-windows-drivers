# GE8300 FW・タイル証拠を含む固定描画診断

`ge8300-render-fw-evidence-manifest.json` はドライバ `dafd822791072dfb3b92cdd13b02124a98cebda8` と今回の固定成果物を示す。GPU描画の命令・タイル設定は141233bのまま。141233bではBG448・前景576・sentinel3072であり、BRN63142配置変更では観測上改善しなかった。今回の目的は未取得のFWとタイル中間情報を同じ一回の試験で得ること。実機動作・署名受理・GPU停止は未検証。

UARTには従来の全16 KiB RGBA（64バイトごと・全体CRC）、色判定、sentinel、fence、CCB、cleanup、shutdownに加え、`FW_EVIDENCE` phase 0／1 と `TILE_SLOTS` を出す。FW観測はHWRカウンタとFWCCBの未処理通知種別を非破壊で数え、rawアドレスを出さない。通知キューのWとW_AFTERが違えば同時更新の可能性がある。タイルはregion headerの16×５バイトとtail pointerの16×８バイトについて、各生スロットの非ゼロ有無だけを同期後・cleanup前に記録する。ゼロ・非ゼロと画面タイル座標の対応は未確定。

`python3 tools/uart/analyze-ge8300-fw-evidence.py UART --offset START --end END` は単一起動・全断片CRC・全体CRC・２回のFW観測・スロット・fence・cleanup・shutdownと色集計を検査する。生RGBAは `tools/gpu/extract-render-pixels.py UART OUTPUT --offset START` で別途取得できる。両コマンドには対象起動のみを含むUART範囲を与える。欠落・混線・同期失敗は不明として再実行等を自動化しない。FWカウンタ０やfence完了、shutdown成功は、HWR不在やハードDMA停止を独立に証明しない。レンダリングの成功は座標・画素の別検証を要する。

WinPEはdrvload・PnP・lifecycleが戻って成功した場合、長い画面送信を自動起動しない。これらの戻り値が失敗した場合は `uart-screen.exe -f` を起動し、画面上の失敗情報を回収する。ホストのUART受信・CRC完了を端末側では確認できないため、成功後のUART欠落時には自動画面フォールバックしない。drvloadが戻らない場合も画面送信は始まらない。試験失敗・timeout時のDMA保持と復旧条件は `README-ge8300-brn63142.md` と `README-ge8300-linear.md` を参照する。

## 再生成・入力検証

`SOURCE_WIM` と `TEMPLATE_ESP` は既存の画面収集実績、`PACKAGE` はmanifestに固定された７ファイル、`CERT` は同署名の公開SerializedCert、`OUT` は新規出力先とする。wimlib、hivex、pyfatfs、fsck.vfatが必要。時刻等により再生成物のハッシュは変わるため、manifestは今回生成したファイルを指す。

```sh
python3 tools/winpe/prepare-ge8300-fw-evidence-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 tools/winpe/add-ge8300-fw-evidence-cert.py --source "$OUT/boot.wim" --source-sha256 "$(sha256sum "$OUT/boot.wim" | cut -d' ' -f1)" --serialized-cert "$CERT" --output "$OUT/boot-trusted.wim"
python3 tools/winpe/verify-ge8300-base.py --base "$SOURCE_WIM" --candidate "$OUT/boot-trusted.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p=pathlib.Path(sys.argv[1]);w=json.loads((p/'boot-trusted.wim.json').read_text())
(p/'stage.json').write_text(json.dumps({'version':1,'token':secrets.token_hex(16),'wim_sha256':w['wim_sha256']})+'\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 80e78974e86c709f0961d4f80db5dcaeff3dade8bf2ab5c86b7102aad4035871 --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot-trusted.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp.img"
python3 tools/winpe/check-ge8300-fw-evidence-inputs.py --test-boot "$TEST_BOOT" --test-esp "$OUT/recovery-esp.img" --restore-boot "$RESTORE_BOOT" --restore-esp "$RESTORE_ESP"
```
