# USB1121既存EXEのread-only witness

`capture-usb1121-existing-exe.py`は、既存captureを変更せず、同Recovery bootで
`Windows/System32/MT8167Usb1121Install198.exe`だけを補足する固定CLI。
このUSB側branchには`capture-peripheral-preservation.py`がないため、同じ
`capture-windows-prebackup.py`のRecovery identity・snapshot・mount検証を再利用した。
GPU側sourceとcontrollerは変更しない。

native235の`captured_named_values`に記録されたサービス
`MT8167Usb1121Install198`のImagePathは、引用符付きの
`C:\Windows\System32\MT8167Usb1121Install198.exe`（REG_SZ）。
この実値を根拠に固定した。capture235を後から補完・上書きした扱いにはしない。

将来の新規captureで使用する場合の呼出形式:

```sh
python3 -B tools/recovery/capture-usb1121-existing-exe.py \
  --manifest "$FRESH_MANIFEST" --manifest-sha256 "$FRESH_SHA256" \
  --out "$NEW_SUPPLEMENT" --include-usb1121-existing-exe
```

既定OFF。opt-in指定がなければ引数エラーで実機I/O前に停止する。
任意のEXE・サービス・remote mount driverのパス引数は受け付けない。
Recovery内の既存`/tmp/rt-ntfs3g`を使い、push・root取得・再起動・Windowsパーティションへの書込みは行わない。
原captureの全ファイル（SYSTEM trio必須）を前後snapshotで照合し、同Recovery bootと
exclusive read-only NTFS mountを確認し、最後にunmount確認を行う。

EXEはpull前に0超・16 MiB以下を確認し、copy後にサイズ/hashを照合する。
欠落時は`missing_files`に明記し`files`へ捏造行を追加しない。
`captured-unmounted`は捕捉処理の完了であり、EXEの存在・rearm admission・実動作成功を
意味しない。利用側は`usb1121_existing_exe.present=true`と固定EXEの実file/hash行を
必ず確認し、必要なrearm witnessへ結合する。

元manifest、native receipt、入力map、サービス設定は変更しない。
本変更時点では構文・CLI parseのみ検証し、実機捕捉は未実施。
