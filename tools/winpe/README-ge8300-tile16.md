# GE8300 16画素単位の終端PPP診断

`ge8300-render-tile16-manifest.json` は `959e1c9` の署名済み固定成果物を示す。既定OFFの64×64診断でのみ、終端PPPの右・下をgeneric Mesaの32画素単位 `0x00040001` からGE8300 SIPF1の16画素単位 `0x000c0003` に変更した。HWRTとPBEの64×64範囲は変えない。UniXNU-a04br3 4a62c22 の機種限定Mesaパッチはvendor GLES経路を根拠とし、同READMEは同じ単位で全画面clear・三角形読戻し成功を記録する。これは本Windows構成の成功を証明しない。

前回同様にcase 0は非対称三角形、case 1はclear-onlyで、独立DMA・VM・contextを使う。危険な先行完了では後段を投入しない。両ケースの全16 KiB画素・部分CRC・全体CRC、sentinel、fence、FW観測、region/TPC、共通shutdownを記録する。`tools/uart/analyze-ge8300-dual-render.py UART --offset START --end END --output-dir EXISTING_EMPTY_DIRECTORY` で一起動を検査し領域別に比較できる。結果はPBE出力の観測であり、位置ごとのISPタイル実行を直接証明しない。FW停止・cleanupはハードDMA停止の独立証明ではない。

22.40向け候補は `android_vendor_benesse_a04br3` revision `0c756f413e7011231c8fb793af0930311023f68d` の `proprietary/vendor/firmware/rgx.fw.22.40.54.30`、SHA-256 `5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1`。リポジトリREADMEはAndroid 10 BSP vendor buildからの生成を説明するが、当該blobの再配布許諾は未確認。firmware情報ヘッダは版1、現Windowsローダーは版3を要求しFWIFも未照合のため、本媒体へ含めず現在の22.67 firmwareを使用する。双方ELF32 MIPSであることはABI互換性を意味しない。

WinPEはdrvload・PnP・lifecycleが成功して戻ると画面送信を省略し、戻り値失敗時のみ画面送信する。UART受信欠落やdrvload停止時は自動画面回収されない。タイムアウト・cleanup失敗等ではDMAを保持する。復旧条件は `README-ge8300-linear.md` を参照。

## 再生成・入力検証

`SOURCE_WIM` と `TEMPLATE_ESP` は既存の画面収集実績、`PACKAGE` はmanifestに固定された７ファイル、`CERT` は同署名の公開SerializedCert、`OUT` は新規出力先とする。wimlib、hivex、pyfatfs、fsck.vfatが必要。時刻等により再生成物のハッシュは変わるため、manifestは今回生成したファイルを指す。

```sh
python3 tools/winpe/prepare-ge8300-tile16-wim.py --source "$SOURCE_WIM" --package "$PACKAGE" --output "$OUT/boot.wim"
python3 tools/winpe/add-ge8300-tile16-cert.py --source "$OUT/boot.wim" --source-sha256 "$(sha256sum "$OUT/boot.wim" | cut -d' ' -f1)" --serialized-cert "$CERT" --output "$OUT/boot-trusted.wim"
python3 tools/winpe/verify-ge8300-base.py --base "$SOURCE_WIM" --candidate "$OUT/boot-trusted.wim"
python3 - "$OUT" <<'PY'
import json, pathlib, secrets, sys
p=pathlib.Path(sys.argv[1]);w=json.loads((p/'boot-trusted.wim.json').read_text())
(p/'stage.json').write_text(json.dumps({'version':1,'token':secrets.token_hex(16),'wim_sha256':w['wim_sha256']})+'\n')
PY
python3 tools/winpe/prepare-mma8652-esp.py --source-esp "$TEMPLATE_ESP" --source-sha256 80e78974e86c709f0961d4f80db5dcaeff3dade8bf2ab5c86b7102aad4035871 --verified-winpe-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d --wim "$OUT/boot-trusted.wim" --stage-manifest "$OUT/stage.json" --output "$OUT/recovery-esp.img"
python3 tools/winpe/check-ge8300-tile16-inputs.py --test-boot "$TEST_BOOT" --test-esp "$OUT/recovery-esp.img" --restore-boot "$RESTORE_BOOT" --restore-esp "$RESTORE_ESP"
```
