# GE8300反復・BQ cold同一起動の候補

これは固定入力から作る未実機検証のWinPE診断であり、通常Windowsや安全な充電を保証しない。`startnet-ge8300-bq-cold.cmd` は既存のGE8300診断と同じ手順で `wpeinit`、automount無効化、GPU firmware配置、GPU `drvload`、PnP・vendor status照会を行う。GPU側で失敗した場合はBQを開始せず、既存の画面送信経路へ進む。GPU statusが成功した場合にBQ cold INFをロードし、battery PnPと残量を照会する。スクリプトの `GE8300_NATIVE_FIRMWARE_DIAGNOSTIC_PASSED` や `BQ_COLD_BATTERY_QUERY_OK_UART_REQUIRED` だけを、反復描画・充電・DMA停止の証明としない。GPU成功はFW応答やソフトウェア結果であり、fabric DMA静止の独立証明ではない。GPU診断が保持するDMAを手動で解放しない。

候補bootは `691f907c6835bcc9992479c29e6aed0eba2f116c` の３ CPU/BQHR/GPU ACPI診断 `boot.img`、SHA-256 `7752660ba55bc205307fc9496fb125c737fd2eb4d4b50210307ea92857dfde44`。対象UEFIは `A04BR3_BQ_HANDOFF=1`、`A04BR3_BATTERY_PROVIDER=1`、`A04BR3_GPU_HANDOFF=1` で、CPU1がwatchdog・UART受信を担当する。実機で同じbootによるBQHR STOPPEDとLC709203F残量は過去に観測済みだが、このGPU反復・BQ coldの組合せは未検証。UEFIの停止record生成コードとWindows分類器は固定ソース同士のホスト結合試験でreason=6、給餌0、fault履歴0、valid mask 0がcold候補になることを確認した。recordのsealは認証ではなく、LKやPnP管理外のI2C利用者の不存在を証明しない。別UEFIを用いた場合、BQHRの欠落・不適合によりBQは拒否される。

当該UEFIのACPIではBAT0はI2C2 `0x1100b000/0x1000` を専有し、IRQを要求しない。GPUは `0x13000000/0x80000`、`0x13ffe000/0x1000`、専有GSIV 217を要求する。アドレスとIRQが重ならないことは、共有クロック・電源・DMA・UARTの安全性の証拠ではない。WIM内SYSTEMは入力のまま、TOPRGU Start=0、driver SHA-256 `52a1a1cb0cd66821099a87db9aca5bd82797061ab0e99a047d495d7fe06c2b3e`。このバイナリには４ CPUでCPU1 ownershipを取得する経路の文字列があるが、今回の３ CPU組合せでの排他は未実証。CPU1 workerとWindowsドライバが同時にwatchdogを所有しないこと、およびUART出力の混線がないことを同一起動のログで確認する必要がある。診断出力中に画面取得コマンドを送らない。安全なUART収集とwatchdog復帰を主担当が確保できない場合は試行しない。

BQ初回I2C読取りは約30秒watchdogを開始し、00hのfault履歴を消費しうる。指定ドライバは既存ポリシーと入力・faultが一致した場合だけ給餌し、それ以外は部分snapshotを残して停止する。新たなLinux充電設定、USB上限変更、HiZ切替はしない。正常所有中のD0終了でのみRESETを試みる。RESETは3.6 V・27分タイマー等のDEFAULTへの変更を伴い、通信失敗や強制リセットではD0終了もRESETも保証されない。充電状態・外部給電・復帰後の再設定は別途確認が必要であり、バッテリー残量や `ACLineStatus` で代用しない。GPUのDMA保持とBQのwatchdogは別々に扱う。

`wpeinit` の後に `mountvol /N` を実行する既存順序を維持するが、これは既にマウントされたボリュームの解放・オフライン化を保証しない。C:の解放が確認されるまでは実機投入しない。基のESPとWIMのほか、復元用boot/ESPの固定値は別途保持する。この候補は元のESPやC:を変更しない。

## 固定入力からの再生成

生成ツールはdriversリポジトリ `461a33f82b92b48fdd6d5aa3f30fcaa9122e2fcf` を使用する。`wimlib-imagex`、hivex、OpenSSL、fsck.vfat、pyfatfsが必要。依存ツールの場所は例で、生成物・一時ファイルの実体は作業ルート内に置く。出力ディレクトリは新規に作る。

```sh
TASK_ROOT=/media/yuma/e109b1f2-7a0a-4242-ba25-6bccbde011b1/work/Android/MediaTek/benesse/TAB-A04-BR3
OUT="$TASK_ROOT/port/xnu/out/ge8300-bq-rebuild"
mkdir "$OUT"
realpath "$OUT"
export TMPDIR="$OUT" PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH=/tmp/a04br3-pyfatfs
export LD_LIBRARY_PATH=/tmp/a04br3-wimtools/usr/lib/x86_64-linux-gnu
export PATH="/tmp/a04br3-wimtools/usr/bin:$PATH"
python3 tools/winpe/prepare-ge8300-bq-cold.py \
  --source-wim "$TASK_ROOT/port/xnu/out/windows-rt-ge8300-start-6fabf08/start.wim" \
  --source-esp "$TASK_ROOT/port/xnu/out/windows-rt-ge8300-start-6fabf08/start-esp.img" \
  --bq-package "$TASK_ROOT/port/xnu/out/bq-cold-3d97d1e/bq-cold-3d97d1e-test-signed.zip" \
  --boot "$TASK_ROOT/port/xnu/out/windows-bqhr-combined/boot.img" \
  --output "$OUT" --wimlib /tmp/a04br3-wimtools/usr/bin/wimlib-imagex
```

入力ESP/WIM、BQ package、bootのSHA-256を固定し、BQ証明書をWinPEのROOTとTrustedPublisherへ登録する。出力WIMを `wimlib verify`、抽出ハッシュ、証明書、SYSTEM hive・TOPRGUを照合し、ESPのBCD/WIM読み戻しと `fsck.vfat -n` を行う。署名は自己署名のテスト用で、対象機での信頼は未確認。GPU firmwareの再配布許諾は未確認。
