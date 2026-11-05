# GE8300 BRN63142・sentinel描画試験

固定入力・成果物は `ge8300-render-brn63142-manifest.json`、ドライバは `141233b462c1fd9103001fd7ca5bcd3142c16e8d`。実機 `22.40.54.30` のvendor `rgxcore_km_22.40.54.30.h` はBRN63142を指定し、`rgxinit.c` と `rgxheapconfig.h` はregion headerを16 GiB境界直前の256 MiB heapに置く。Mesa/Linuxの `pvr_rogue_heap_config.h` も同じ `0xdbf0000000` を指定している。従来の `0x8000046700` はこの条件を満たしていなかった。所有DMA中の新しい１ページをそのVAへマップし、HWRTに設定する。32×32欠落との因果関係および修正後の描画は未実証。

描画前に出力面のみ `a5 5a c3 7e` のRGBA sentinelで埋める。元の未使用・ゼロ・所有・PTE検査を通してから変更し、既存の全DMA同期後にkickする。対象以外の領域は変更しない。UARTの `SENTINEL` 個数とCRC付き16 KiB画素を照合する。sentinel残存は未書込みと整合するが、GPUが同値を書いた場合や可視性不良を排除しない。ゼロは少なくともsentinelのままではない。残りのfence・lifetime・FW停止とCRC復元方法は `README-ge8300-rgba.md` を参照する。

WinPEはdrvload、PnP、lifecycleが戻り成功した場合、時間のかかる画面送信を自動起動しない。準備・drvload・PnP・lifecycleの戻り値が失敗した場合は、従来の `uart-screen.exe -f` による画面送信を起動する。画面には各段階の戻り値が残り、同じ起動内で画面コマンドも利用できる。drvloadが戻らない場合はフォールバックも始まらない。端末はホスト側CRC受信の成否を認識できないため、lifecycle成功後にUARTが欠落した場合の画面送信は自動では起動しない。画素CRC未完了を成功扱いしない。

既存のテスト証明書と同様、公開テスト証明書をWIMのRoot／TrustedPublisherへオフライン登録している。実機での署名受理、GPU動作、UART完走、FWのHWR／context reset発生有無とDMA停止は未検証。timeout・cleanup失敗時のDMA保持と復旧制約は従来どおり。GPU試験を繰り返す前に取得済みの画素・UART・状態を解析する。

## 再生成

`README-ge8300-rgba.md` の同じ入力と手順を使い、WIM生成を `prepare-ge8300-brn63142-wim.py`、証明書登録を `add-ge8300-brn63142-cert.py`、４入力検証を `check-ge8300-brn63142-inputs.py` に置き換える。署名７ファイル、SerializedCert、入力WIM・ESP、今回の出力はmanifestのハッシュに固定する。固定された今回の出力と再生成物はタイムスタンプ等により異なり得る。
