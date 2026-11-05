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

listenerはwpeinitとmountvol /Nの成功直後、共有親・touch・GPUのdrvloadより前にバックグラウンド起動する。起動指示は準備完了を保証しない。画面送信はUARTを排他オープンし、listenerのオープンと衝突するため自動起動しない。vendor statusのUART送信も自動実行しない。手動の画面送信中はlistenerがUARTを開けず、要求の回収と応答が遅れる可能性がある。listenerは短時間の共有オープンと200ms間隔で要求を回収し、ドライバ内の単一送信bufferのbusy時は応答を再試行する。READY報告は30秒の送信失敗でも内部的に送信済みとなり再通知されない。GPUの直接MMIO出力はこの排他に参加しないため文字混在や応答欠落があり得る。`A04VOL_LISTENER_READY` の後、`A04VOL_RELEASED` の完全な受信だけをC:解除成功とする。解除確認前の実機書込みは禁止。C:解除後のresetとGPU DMAの停止は別問題であり、reset時のDMA安全性は未証明。thermal handoffは観測のみで所有解放を意味せず、現在のWinPEでは未実機確認。surprise removal時のMMIO、fabric DMA停止、cache可視性、WDF参照による資源寿命は未証明。専用bufferと診断の成功は一般DMA静止や画面共有の安全性を証明しない。

## READY反復版listenerへの交換

`replace-volume-listener-copy-wim.py` は上記のthermal付き早期listener媒体を入力に、署名済み `740117c` パッケージのlistenerのみ交換し、別媒体を作る。`--source-wim`、`--source-esp`、`--boot`、`--package`、`--output` を指定する。入力と出力は実体を確認した作業ルート内とし、outputは空ディレクトリを使う。WIMとESPの照合、既存証明書・SYSTEM・GPU・startnetの保持とlistenerの再抽出を検証する。

`740117c` の署名済みパッケージはGit bundleに固定したコミットから生成された。生成当初はGitHubのcommit_refs拒否で未公開だったが、2026-11-06に公開を確認した。listenerは要求までREADYを繰り返すが、UARTの混線・欠落は引き続き失敗または不明。GPUの直接出力との排他保証や、解除後のGPU DMA停止は確立していない。
