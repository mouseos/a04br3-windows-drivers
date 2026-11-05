# GE8300 二条件描画診断

固定成果物は `ge8300-render-dual-manifest.json`、ドライバは `3e5e0f77a4b80f592b74c2d19cf7d0ff7828e18d`。実機未検証。case 0は既存の非対称三角形、case 1はMesaキャプチャのterminate-only VDMを用いたclear-onlyで、別DMA・VM・contextを使用する。前段のfence、cleanup、FW観測または完了状態が危険ならcase 1を投入しない。画素不一致のみの場合は次へ進む。双方のDMAはFW停止まで保持する。

case 1だけ全領域が背景となりcase 0が不全ならgeometry／vertex／PPP経路が候補となる。双方同様なら共通背景／ISP／PBE／可視性等が候補となるが、単独では原因を確定できない。各caseでsentinel初期化と全16 KiB RGBA、部分・全体CRC、fence、FW観測、region/TPCを取得し、最後に共通shutdownを記録する。sentinelの残存だけでGPU非書込みは断定できず、FWカウンタやshutdown成功もハードDMA停止の独立証明ではない。

`python3 tools/uart/analyze-ge8300-dual-render.py UART --offset START --end END --output-dir NEW_DIRECTORY` は１起動内の２case、断片・全体CRC、順序、fence、shutdownを確認し、画素を16×16領域別に分類し、検証後にRGBAを出力する。case欠落、SKIPPED、ABORT、CRC不足は不完全扱い。出力先は新規ディレクトリを指定する。通信欠落やdrvload停止時には画面送信が自動起動しない。drvload・PnP・lifecycleが失敗として戻った場合のみ画面を送信する。

タイムアウト・cleanup失敗等ではDMAを保持する。watchdogやUART reset、FW停止はGPU DMA停止を保証せず、確実なハードリセットと所有状態の確認が必要。復旧条件は `README-ge8300-linear.md` を参照。

## 再生成・入力検証

`SOURCE_WIM` と `TEMPLATE_ESP` は既存の画面収集実績、`PACKAGE` はmanifestに固定された７ファイル、`CERT` は同署名の公開SerializedCert、`OUT` は新規出力先とする。wimlib、hivex、pyfatfs、fsck.vfatが必要。時刻等により再生成物のハッシュは変わるため、manifestは今回生成したファイルを指す。

```sh
python3 tools/winpe/prepare-ge8300-dual-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 tools/winpe/add-ge8300-dual-cert.py --source "$OUT/boot.wim" --source-sha256 "$(sha256sum "$OUT/boot.wim" | cut -d' ' -f1)" --serialized-cert "$CERT" --output "$OUT/boot-trusted.wim"
python3 tools/winpe/verify-ge8300-base.py --base "$SOURCE_WIM" --candidate "$OUT/boot-trusted.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p=pathlib.Path(sys.argv[1]);w=json.loads((p/'boot-trusted.wim.json').read_text())
(p/'stage.json').write_text(json.dumps({'version':1,'token':secrets.token_hex(16),'wim_sha256':w['wim_sha256']})+'\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 80e78974e86c709f0961d4f80db5dcaeff3dade8bf2ab5c86b7102aad4035871 --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot-trusted.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp.img"
python3 tools/winpe/check-ge8300-dual-inputs.py --test-boot "$TEST_BOOT" --test-esp "$OUT/recovery-esp.img" --restore-boot "$RESTORE_BOOT" --restore-esp "$RESTORE_ESP"
```
