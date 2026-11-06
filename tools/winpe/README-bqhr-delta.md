# BQHR読み取り差分の統合手順

この差分はCPU/USB共有親、PnP/容量監査、タッチ、GPU、TOPRGU、UEFIを置換しない。Windows ARM32自己署名の `bqhr-observe.sys`、`bqhr-observe.inf`、`bqhr-observe.cat`、`usb-bqhr-uart.exe`、`bqhr-observe-test.cer` が必要。`manifest.json` の各SHA-256を照合する。署名済みであっても端末の信頼・PnP・動作は未確認。

対象UEFIに `A04BR3_BATTERY_PROVIDER=1`、`A04BR3_BQ_HANDOFF=1` を確認する。基準のCPU媒体はUEFI `e969a116` で両方1。BQHRは `\\_SB.BAT0.BQHR` の128バイト記録であり、UEFIのACPI NVSから得る。UEFIやWindowsが実際にメソッドを公開・評価するかは起動ごとに未確認。

このドライバは `ACPI\\MOU0008` の機能ドライバとしてPnPバインドされ、燃料計の機能ドライバとは同時に使えない。対象のPnP構成とOS担当の監査構成を確認し、MOU0008で他の機能ドライバを必要とする場合はこの差分を起動しない。既存バッテリー機能を引き継がず、I2Cはマップ・読取り・書込みしない。PnP上はリソースを占有しうる。BQHR専用のフィルタではない。

媒体作成時にコピーする。

- `bqhr-observe.inf`、`bqhr-observe.sys`、`bqhr-observe.cat` → `X:\\a04br3\\usb-bqhr\\`
- `usb-bqhr-uart.exe` → `X:\\a04br3\\`
- `bqhr-observe-test.cer` → 作成するWIMの既存証明書を残したまま、証明書のサムプリントとBlobをオフラインのSOFTWARE hiveの `Microsoft\\SystemCertificates\\Root\\Certificates` および `TrustedPublisher\\Certificates` に追加し、書込み後に読み戻して照合。既存のCPU/USB親用証明書を削除・置換しない。OS担当の証明書統合作業に同梱し、実機Windowsのストアへ直接インストールしない。ストアへの追加はRTでの署名受理を保証しない。

OS担当のstartnetを優先して保持する。MOU0008競合がないと判断した構成のみ、既存の共有親・タッチ・USB観測が開始された後、CPUのUART連続観測開始前の順序を保てる位置に以下を追加する。既存のPnP・容量監査を削除・移動せず、UART利用が同時になる場合は連続送信が終わる位置へ調整する。BQHR失敗時もCPU・USB・GPUの診断は続行する。

```bat
echo A04BQHR_DRVLOAD_START
drvload X:\a04br3\usb-bqhr\bqhr-observe.inf
echo A04BQHR_DRVLOAD_EXIT=%errorlevel%
X:\a04br3\usb-bqhr-uart.exe
echo A04BQHR_OBSERVATION_EXIT=%errorlevel%
```

`drvload` 成功はPnP開始を意味しない。ツールはTOPRGU UARTを最大30秒待ち、BQHRデバイスをさらに最大30秒待つ。`A04BQHR SNAP <id> 0 <result> <NTSTATUS> 0 0` のresult 0はACPI評価失敗、1は形式不正、2は記録不正、3は構造上STOPPED、4はその他の構造上有効な状態。FFFFFFFFはBQHRデバイスを開けず、FFFFFFFEはWindows IOCTL取得失敗。result 3/4では同じIDの1〜8行が128バイトの生記録を表す。生ログは `python3 decode-usb-bqhr.py UARTログ` で復元できる。欠損・矛盾した重複・不正なsealは拒否する。UART成功はキュー登録であり、欠落・混線がありうる。

結果はUEFIの記録であり、Linuxの引継ぎ記録、実時間の充電IC値、ファームウェア/EL3の所有権解放、watchdogの維持、VCDT電圧、USB給電の証明ではない。BQHR停止とCPU・USB・タッチ・GPUの結果は独立に評価する。実機アクセス・書込みは主担当のみ。
