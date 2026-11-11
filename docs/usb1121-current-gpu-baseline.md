# 搭載中GPUを保持するUSB1121入力

`tools/ci/prepare-usb1121-current-baseline.py` は既USB component入力生成器に
渡すbaseline policyを作る。GPUの更新やGPU診断の起動は行わない。
既 `prepare-usb1121-service198.py` の `admit_native` / `bind_gpu` を再利用する。

map schemaは `USB1121_CURRENT_GPU_BASELINE_INPUTS_V1`。
`fresh`, `native`, `supplement`, `actual_package_rows`, `GPU_manifest` は
それぞれ作業root内の実 `path` / `sha256` 参照とする。
`no_intervening_device_write_reported_by_main` は主担当の取得後記録であり、
推測でtrueへ変更しない。全13roleの実package行、全旧service、全named値、
原SYSTEM trio、同bootの補足を保持する。

通常実行は履歴参照の検査だけで、実機I/Oも配備用policy生成も行わない。
書込み後の旧captureは `no_intervening_device_write_reported_by_main=false` の
まま検査できるが、配備への再利用は拒否する。

```sh
python3 -B tools/ci/prepare-usb1121-current-baseline.py \
  --workspace "$ROOT" --map "$ACTUAL_MAP" --out "$NEW_PRIVATE_REVIEW"
```

新しく実取得したcaptureを配備に使用する場合だけ `--finalize` を追加する。
その場合は既 `capture-windows-prebackup.py verify` で同Recovery boot、
serial、partition layout、unmounted状態を再確認してから
`current-policy.json` を出力する。これはRecovery内の介在書込みを自動検出する
全ディスク監査ではない。実manifestとその原本ファイルは別途全hash照合する。

GPUのSYS・UMD・FWとtransport SYSは、実取得ファイル・nativeの選択版数/INF・
実署名manifestの全leafへ同時に一致しなければならない。たとえば379取得に
380候補を指定しても通らない。未更新GPUの将来値をbaselineに埋めない。
更新しない他roleのtrioも取得SYS・保持ファイルの実byteへ結合する。

出力policyとそのhashを既component生成器の `--final-policy` /
`--final-policy-sha256` に渡し、`--retain-current-gpu` を指定する。
GPU補足を取得している場合は同じ実 `--gpu-capture-manifest` /
`--gpu-capture-sha256` も渡す。profileは `standalone-retained-gpu` となる。
USB bundle/helper/driverの検査、1TUの署名、
既存サービスの明示rearm、媒体のbaseline比較、U198の同run/PIDに結合した
SCM停止・flush・terminal確認はそれぞれ既入口で実施する。
このpolicy生成だけをPnP、USB host列挙、給電の成功と扱わない。

既serviceの再有効化と媒体は [既存service rearm手順](usb1121-current-existing-rearm.md)
へ接続する。旧サービス全体の保持検査を外さず、既存Startだけを4から2へ変更する。
GPUのrender-only transactionの末尾にUSBの自動再起動を混ぜない。
