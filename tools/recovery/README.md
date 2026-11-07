# recoveryからの実験用ドライバ更新

`install-button-probe.py` は別の一回限りの実験です。WindowsのSYSTEMハイブを事前退避し、診断サービスの5値以外の変更がないことを `python-registry` で比較してから、診断EXEと候補ハイブをファイル単位で書きます。ホストに `python-registry` と `reged` が必要です。Windowsが診断を実行できることやサービスの自動無効化は実機で確認する必要があります。導入失敗時には保存した元ハイブを保持し、原因を調べます。

`update-driver-ntfs.py` は同一INFのテスト署名済みブートドライバを、全Windowsイメージを書き直さずに更新する実験手順です。既存の正しいインストールとrootのAOSP recovery、端末で動く静的NTFS-3Gが必要です。指定のWindows領域位置、未マウント状態、転送ハッシュ、既存INFとの一致を確認し、変更前のSYSとCATを指定先へ保存してから、既存ファイルの内容を更新します。GPTや他領域は書きません。

```sh
python3 tools/recovery/update-driver-ntfs.py \
  --package /path/to/test-signed-package \
  --ntfs3g /path/to/static-aarch64-ntfs-3g \
  --backup-dir /path/to/new-backup-directory
```

テストモードの実機ではこの方法で置換したブートドライバのDriverEntryと初期化を確認しました。カタログデータベース全体の整合性や、通常署名の受理は未検証です。失敗時のバックアップを保持し、起動結果を確認してください。正式なドライバインストールや署名を置き換える製品手順ではありません。

静的NTFS-3Gの作成例: Debian提供の `ntfs-3g_2022.10.3.orig.tar.gz`（SHA-256 `8bd7749ea9d8534c9f0664d48b576e90b96d45ec8803c9427f6ffaa2f0dde299`）を取得し、Autoconf、Automake、Libtool、libgcrypt開発用マクロとAArch64 GNU/Linuxクロスコンパイラを用意します。ソースに含まれるGPL等の条件に従ってください。

```sh
tar -xzf ntfs-3g_2022.10.3.orig.tar.gz
cd ntfs-3g-2022.10.3
./autogen.sh
CC=aarch64-linux-gnu-gcc ./configure --host=aarch64-linux-gnu --disable-shared --enable-static --disable-crypto
make -j4 LDFLAGS=-static
cd src
aarch64-linux-gnu-gcc -static -o ntfs-3g-static \
  ntfs_3g-ntfs-3g.o ntfs_3g-ntfs-3g_common.o \
  ../libfuse-lite/.libs/libfuse-lite.a -lpthread ../libntfs-3g/.libs/libntfs-3g.a -lc
```

リンク時の警告も確認し、対象recovery上での実行と使い捨てNTFSでのマウント・書込みを確認してから使用します。NTFS-3Gは未クリーンなボリュームを修復する場合があり、処理結果をログで確認してください。

## Recoveryでの更新前ホスト退避

`capture-windows-prebackup.py` は、serial固定・Recovery/UID0・Windows partition検査・ro mountの下で、SYSTEM/SOFTWARE/DRIVERSと各LOG1/LOG2、指定SYSをホストの新規private directoryへ回収し、SHA-256/長さを照合してunmountする。書込み・再起動は行わない。成功manifestは `captured-unmounted`。writer直前のread-only verify modeで固定manifest hashと同一Recovery boot IDを照合する。Windows起動や別の変更を挟むと旧バックアップは更新前証拠として失効する。privateデータをESP・公開repoへコピーしない。詳細は [README-windows-prebackup.md](README-windows-prebackup.md) を参照。
