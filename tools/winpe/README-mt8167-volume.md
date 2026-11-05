# MT8167 WinPE: UARTボリューム解除要求

`prepare-mt8167-volume-wim.py` は共有MT8167/BU21025診断WIMへ、既定OFFのUART解除要求を有効にしたTOPRGU、固定操作リスナー、ボリューム識別ガード、解除batchを組み込む。基のBU21025用署名証明書を保持し、新しいテスト証明書をWinPEのコピーにのみ追加する。既存SYSTEM hiveのハッシュとTOPRGUのboot-startを検査する。

画面送信が戻った後にリスナーを起動する。ホストは `A04VOL_LISTENER_READY` を確認後、UARTへ `A04BR3!VOL` を1回送る。`A04VOL_RELEASED` の完全な受信だけを、対象をガードして `mountvol C: /p` が成功したという報告として扱う。存在しないC:、別のボリューム、registryの不明状態、署名拒否、送信欠落や処理時間超過は失敗または不明であり、そのままresetしない。TOPRGU・リスナーのWindows実行、署名受理、UART要求、ディスク解除は未実機検証。現在稼働中の旧WinPEではこのコマンドは使えない。

作業ルート内の空ディレクトリを出力先とし、`TMPDIR`をルート内へ設定する。依存はwimlib-imagex、hivexsh、hivexget、OpenSSL、fsck.vfat、Python pyfatfs。固定SHA-256の入力だけを受け付け、WIM verify、各ファイル、証明書、既存SYSTEM、ESPのFATと中身を検査する。

```sh
python3 tools/winpe/prepare-mt8167-volume-wim.py \
 --source-wim "$ROOT/port/xnu/out/windows-rt-mt8167-bu21025-dd35a76/shared.wim" \
 --source-esp "$ROOT/port/xnu/out/windows-rt-mt8167-bu21025-dd35a76/shared-esp.img" \
 --package "$ROOT/port/xnu/out/windows-rt-toprgu-volume-0190f2d/signed.zip" \
 --boot "$ROOT/port/xnu/out/windows-rt-uefi-shared-bqhr-a06afed0/boot.img" \
 --output "$OUTPUT"
```

ホストの `tools/uart/request-winpe-volume-release.py` に診断起動前に保存したUARTログのバイト位置を指定すると、その位置以降のREADYを待ち、要求を送信して結果を監視する。`--reset-after-release` は成功行を完全に受信した場合だけresetを要求する。ログの切替・切詰め、失敗行、タイムアウトではresetしない。旧WinPEにはREADYがないためこのツールでは解除できない。
