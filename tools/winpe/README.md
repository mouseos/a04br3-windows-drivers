# WinPEディスク診断

ユーザーが用意したMicrosoft Surface RT公式回復媒体から、入力不要の診断用ESPを作る。公式 `boot.wim` と残りの回復ファイルを別途用意する。元の媒体は変更せず、OSバイナリや作成済み画像はリポジトリへ含めない。スクリプトは `wpeinit` 後、DiskPartの `list disk` と `list volume`、SDサービスの登録状態を表示する。ディスクの変更はしない。

以下では `RECOVERY_FILES` に公式EFI/Boot/Bootフォルダを含む抽出済みディレクトリ、`ORIGINAL_WIM` に公式boot.wim、`OUTPUT_DIR` に新規作業ディレクトリの絶対パスを指定する。`RECOVERY_FILES` は `build-recovery-esp.py` が検査する87ファイルの媒体を対象とする。Linuxにwimlib-imagex、dosfstools、Python venvを導入する。

```sh
python3 -m venv "$OUTPUT_DIR/venv"
"$OUTPUT_DIR/venv/bin/pip" install -r tools/winpe/requirements.txt
cp "$ORIGINAL_WIM" "$OUTPUT_DIR/boot.wim"
printf 'delete /Windows/System32/winpeshl.ini\nadd %s /Windows/System32/startnet.cmd\n' "$PWD/tools/winpe/startnet.cmd" | wimlib-imagex update "$OUTPUT_DIR/boot.wim" 1
wimlib-imagex verify "$OUTPUT_DIR/boot.wim"
WIM_SIZE=$(stat -c %s "$OUTPUT_DIR/boot.wim")
"$OUTPUT_DIR/venv/bin/python" tools/winpe/build-recovery-esp.py --files "$RECOVERY_FILES" --boot-wim "$OUTPUT_DIR/boot.wim" --expected-wim-size "$WIM_SIZE" --description '診断用の派生WinPE' --output "$OUTPUT_DIR/recovery-esp.img"
```

wimupdateのコマンド形式には空白を含まない作業パスを使う。作成後はWIMの抽出確認、FAT内のハッシュ照合およびfsck結果を確認する。画像を端末に書く場合は使用機のGPT配置と復旧手段を別途確認する。

実機ではWindows側のDiskPartが固定ディスクとボリュームを列挙しなかった。これはこの端末の現在のACPI・ドライバ構成での結果であり、他端末の結果を意味しない。元のWinREはMicrosoftの指定するSurface向けであり、本端末へのライセンスや再配布権を意味しない。

## 署名・読込み診断（未完成ドライバ）

別の診断 `startnet-driver-probe.cmd` は `/a04br3/a04-msdc-probe.inf` を `drvload` し、終了コードとサービスの登録結果を画面に表示する。ドライバは常にアダプターの検出を拒否し、ディスクI/Oは行わない。WinPEがドライバを受理するかの調査用で、成功してもストレージが動くことは意味しない。現時点のSYS・CATは未署名であり、署名ポリシーによる拒否があり得る。

`tools/ci/build-wdk81-probe.ps1` をWDK 8.1 ARMとClang/LLDのあるWindows環境で実行すると、`$env:RUNNER_TEMP/a04br3-wdk81-probe/package` にSYS・INF・Windows 8.1 ARM用の未署名CATを作成する。前述のWIM更新で使用する `startnet.cmd` をこの診断版に替え、同ディレクトリの3ファイルをWIMの `/a04br3/` に追加してESPを作成する。ファイルの抽出比較とWIM・FATの検証を行う。媒体・イメージは配布しない。

未署名カタログとの比較試験には、同じランナーで `tools/ci/sign-probe-test.ps1` を実行する。診断用に一時自己署名証明書を作りCATを署名し、公開証明書だけをパッケージへ出力する。秘密鍵はランナー上に留まり、作業終了時に失われる。証明書を信頼済みとして登録する処理は含まず、Windows RTがロードを許可する保証もない。元の未署名パッケージを保存してから比較する。

INFは改行コードが変わるだけでCAT内のハッシュと一致しなくなる。**Windows上でCAT生成したパッケージのINF・SYS・CATを一組で回収し、変更せずに使う。** `prepare-driver-probe-wim.py --source-wim ORIGINAL --package PACKAGE_DIR --output OUTPUT` はその3ファイルを一組で追加し、WIMの検証と抽出後のバイト一致を確認する。別checkoutのINFを組み合わせない。
