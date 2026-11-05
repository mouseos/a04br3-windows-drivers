# GE8300 m1.11 コピー同一起動候補

`f2a2b5c` の固定32×32 scratchコピーを、MT8167共有親・BU21025・UART解除の既存WinPE媒体に追加する実験。一般のWDDM表示やscanoutではない。GPUコピー、同一起動、署名受理は実機未確認。元のファームウェアとREAで抽出したUSC assetを使用する。

WDK 8.1 ARM32では `tools/ci/build-ge8300-provider.ps1` に対し `A04_GE8300_VENDOR_STAGE`、`A04_GE8300_VENDOR_UNCACHED_STAGE`、`A04_GE8300_VENDOR_IRQ`、`A04_GE8300_VENDOR_START_DIAGNOSTIC`、`A04_GE8300_VENDOR_RENDER_COLD`、`A04_GE8300_VENDOR_COPY_DIAGNOSTIC` を1にし、clear・screenは0にする。`A04_GE8300_COPY_USC` は元libsrv_um.soからREAで得た152バイトの実体を指定し、SHA-256は `5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3`。旧 `A04_GE8300_COPY_TEST` は別経路なので0。対象firmwareのSHA-256は `5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1`。statusツールのビルド後に `tools/ci/sign-ge8300-test.ps1` を実行する。自己署名証明書はビルドごとに変わるため、以下の再生成には固定済みsigned.zipが必要。

固定入力の媒体生成例。作業ルート内の空ディレクトリを用意し、`wimlib-imagex`、`hivexsh`、`hivexget`、`openssl`、`fsck.vfat` とPythonの `pyfatfs` をPATH等に設定する。TMPDIRも作業ルート内を指定する。

```sh
python3 tools/winpe/prepare-ge8300-copy-volume-wim.py \
  --source-wim "$BASE/volume.wim" --source-esp "$BASE/volume-esp.img" \
  --boot "$BASE/boot.img" --package "$SIGNED_ZIP" --output "$OUT"
```

入力はハッシュ固定。WIM内のSYSTEMと既存証明書を維持し、新しい証明書をRootとTrustedPublisherに追加する。パッケージとstartnetを組み込み、WIM検証・再抽出照合、ESP内WIMとBCD照合、FAT検査、manifest生成を行う。作業ルートと入力実体は実行前に `realpath` で確認する。

GPUの `drvload` またはstatusが停止すると後続の画面収集と解除listenerが起動しない。`A04VOL_LISTENER_READY` の後、`A04VOL_RELEASED` の完全な受信だけをC:解除成功とする。解除確認前の実機書込みは禁止。surprise removal時のMMIO、fabric DMA停止、cache可視性、WDF参照による資源寿命は未証明。専用bufferと診断の成功は一般DMA静止や画面共有の安全性を証明しない。
