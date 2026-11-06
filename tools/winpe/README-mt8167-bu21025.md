# MT8167 / BU21025 WinPE診断

共有親とBU21025の自己署名パッケージを、既存WinPEのコピーに組み込む。UEFIは `a06afed0` の共有APMIXED・BQHR停止記録構成、タッチ側は `81b550e` の共有親用で、従来タッチproviderと併用しない。生成物は実機未検証。`drvload` の成功は署名受理後の要求完了を示すだけで、PnP開始や画面上のタッチ動作を証明しない。共有GPIO・PMIC・クロックの所有とwatchdog引継ぎは未確認。

`mountvol /N` は既存マウントの解除を保証しない。起動中のWindows領域の解放が未確認の現在は、端末のreset・書込みの根拠にしない。WinPEではC:の読書きを要求せず、署名証明書は派生WIMのみに追加する。

作業ルート内の空ディレクトリを出力先として指定し、`TMPDIR`を同じ作業ルート内に設定する。`wimlib-imagex`、`hivexsh`、`hivexget`、`openssl`、`fsck.vfat`、Python `pyfatfs` が必要。入力のSHA-256をスクリプト内で固定し、WIM verify、組込みファイル・証明書照合、ESPの内容とFATの検査を行う。ビルド元・ハッシュは出力manifestを参照。

```sh
python3 tools/winpe/prepare-mt8167-bu21025.py \
  --source-wim "$ROOT/port/xnu/out/windows-rt-ge8300-start-6fabf08/start.wim" \
  --source-esp "$ROOT/port/xnu/out/windows-rt-ge8300-start-6fabf08/start-esp.img" \
  --package "$ROOT/port/xnu/out/windows-rt-shared-touch-81b550e/signed.zip" \
  --boot "$ROOT/port/xnu/out/windows-rt-uefi-shared-bqhr-a06afed0/boot.img" \
  --output "$OUTPUT"
```

GPUとBQのテストは開始せず、共有親・タッチのdrvload後にUART画面送信を試みる。画面転送は物理画面と同一である保証はない。署名のWindows受理、PnP、画面位置、連続入力、スリープ復帰は別々に確認する。入力となる旧WIMにはGE8300由来ファイルが残るため、その配布許諾も未確認。
