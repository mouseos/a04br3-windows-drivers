# 実配置MT6630 provider1027の受動UART観測

`tools/host/observe-mt6630-1027-uart.py` は既collectorが保存するregular fileを
read-onlyで一回snapshotするhost専用tool。VM/実機/UART deviceへ接続せず、
scan・接続・再起動・client/profile起動を行わない。driver/媒体の変更は不要。
基点は実provider1027 source73427b486f3e621dd998774f38f396fbac54a61d。
新1028候補を適用するためのtoolではない。

## 観測できる出力と限界

- `tools/winpe/mt6630-wlan-probe.c:50` の既 `A04WLAN RUN=… SEQ=… IF=… LABEL=…`。
  LABELは同sourceが出す固定allowlist、valueはDWORDだけ。
  この既probe自体はscan専用で接続APIを呼ばない。toolはprobeを実行しない。
- `tools/winpe/mt6630-pnp-uart.c:42` の `A04WIFI PNP=… MATCHES=… STATUS=… PROBLEM=…`。
  PnP statusの観測で、scanや接続成功の証明ではない。
- 同`:49` の `A04WIFI FW WMT=… ERR=… WLAN=… ERR=…`。
  firmware file size/GetLastErrorであり、firmware起動成功の証明ではない。

`drivers/network/mt6630/checkpoint.c:29` のDriverEntry/Initialize/Scan/RuntimeFailureは
NativeCheckpointレジストリのREG_BINARYへ保存される。`:115` のstage/phase/kind/raw/detailは
UARTへemitされない。既1027 `ndis_runtime.c` からも接続段階のUART出力はない。
そのためUART-onlyでknown checkpoint stage/NTSTATUS/OIDを抽出することはできない。
出力の `checkpoint_stages` / `NTSTATUS_events` は空にし、架空のCP形式は解釈しない。
既export済REG_BINARYが別途ある場合は既 `tools/winpe/decode-mt6630-checkpoint.py` の用途で、
本toolはhiveへアクセスしない。レジストリ再採取やVM作業も行わない。

scan callback・BSS一覧観測はscanとしてのみ記録する。
`OPEN_ERROR` はWlanOpenHandleの戻り値で、802.11 authenticationではない。
association / authentication / key / controlled_port / IPは専用UART証拠なしの未検証を維持する。
SCAN_EXIT/END/PnPの0、FW size、PB terminalを通信成功へ変換しない。
Wi-Fi一覧が表示されたという利用者証言は別の観測であり、toolが再観測したことにしない。

## mainのhost実行例

```sh
ROOT=/media/yuma/e109b1f2-7a0a-4242-ba25-6bccbde011b1/work/Android/MediaTek/benesse/TAB-A04-BR3
TOOL="$ROOT/windows-rt/a04br3-windows-mt6630-1027-host-observer/tools/host/observe-mt6630-1027-uart.py"
python3 -B "$TOOL" --workspace "$ROOT" \
 --uart-file "$ROOT/port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log" \
 --output "$ROOT/port/gsi/logs/ios-feasibility/NEW-observation.json"
```

既定は実行開始時点の末尾8MiBまでで、file増加を待たない。
mainが試験開始時点のbyte位置を保持していれば、`--start-byte N --max-bytes 8388608` で
その範囲だけを読む。先頭が途中行の可能性を避けるためstart>0の最初の行は捨てる。
`--max-bytes` は最大128MiB。既存出力は上書きせず、新規JSONをmode0600で保存する。
root外file、device/FIFO/socket、root外出力は拒否する。
入力と出力のpathをreportへ含めない。原本UARTを複製せず、hashや原文も保存しない。
既知numeric record以外はすべて捨て、SSID/鍵/MAC/IP/GUID等の自由文字列は出力しない。
既知storage3patternの混入だけ除去し、未知の混入を推測修復しない。

timelineはfile内順序で、source_byte_upper_boundは読取りchunk終端の上限であり厳密な各行offsetではない。
wall clock時刻や同bootを合成しない。RUNはGetTickCount由来なのでboot identityではない。
履歴・重複probeを接続試験の再実行と断定せず、4096event上限到達も明示する。
イベント欠落は「接続試験未実施」や「driver失敗」の証明ではない。

## host検証

```sh
python3 -B tests/mt6630_uart_observer_test.py
```

scan成功と接続の分離、WlanOpenHandle error分類、PnP/FW限定分類、未知CP/接続成功風lineの拒否、
秘密文字列の非出力、partial/overlong line、既知混入、snapshot byte境界、出力上書き拒否を確認する。
実機通信やNDIS動作の証明には使わない。
