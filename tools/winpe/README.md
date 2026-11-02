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
