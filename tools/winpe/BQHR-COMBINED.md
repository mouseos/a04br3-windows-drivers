# BQHR・燃料計・eMMCの同一起動診断

これは3 CPU試験用WinPEであり、通常OSや配布用イメージではない。UEFIのBQ所有開始条件は常に不成立で、BQ24160への読取り・給餌は行わない。`BQHR` はACPI NVSの停止recordだけを読む。CPU1のUEFI workerが復旧watchdogとUART受信を所有するため、WinPEからTOPRGUドライバを別途起動しない。自動復旧や各機能の実機動作はこのイメージの作成だけでは証明されない。

## 同一起動での順序と判定

1. `wpeinit` 後にautomountを無効にする。`bqhr-test-cert.exe` はSystemDriveがX:で、HKLMの `SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinPE` キーがあるWinPE以外では拒否し、同梱の開発用証明書をWinPEの一時的なローカルマシンROOTとTrustedPublisherに追加する。証明書は実運用の信頼を示さない。
2. 署名済みBQHR診断付きLC709203Fをロードし、PnPと残量を照会する。`BATTERY_QUERY_OK` と `BATTERY_PERCENT_HEX` が残量取得の証拠。ACLineStatusは給電判定に使わない。
3. BQHRはドライバD0開始時のACPIメソッド評価であり、同じ起動のUARTに `A04BAT BQHR STOPPED VERIFIED` があることを別途確認する。`BQHR_REQUIRES_UART_CONFIRMATION` という画面表示自体は成功を意味しない。メソッド不在・不正・想定外stateは `A04BAT BQHR FAILED`。古い起動のUARTを混ぜない。
4. 保存済み署名済みread-only MSDC SYS/INF/CATをロードし、既存の署名済みSHA付き計測EXEを実行する。`MSDC_READ_STRESS_PASSED` に加えLBA `0xE1000` から16 MiBのSHA-256がAndroid recoveryで記録した `473ec59979015aa168b251f3d31892baf325526adc85e61e04e6538ae74ba0f1` と一致した場合のみ `MSDC_REFERENCE_MATCHES_ANDROID_RECOVERY` を出す。別起動等で対象範囲が更新されていれば基準が古くなるため、不一致を直ちに読み取り障害と断定しない。
5. 結果はWinPE RAMドライブ `X:\a04br3\battery-result.txt` と `msdc-result.txt`、画面、UARTに出る。いずれかが失敗すれば後続を実行せず、マーカーを出して終了する。明示されたBQHR UART確認・残量・ストレージの結果をそれぞれ判定する。実機ストレージの書込み・修復コマンドは含まない。

## WIM作成

入力は事前取得済みの `windows-rt-multiblock-run3/stress.wim`、同じ試験に使用した `windows-rt-multiblock-5ec0aaa/package`、`msdc-sha256-stress-signed.exe`、BQHR診断とUART traceを有効にしBQ chargerを無効にした署名済みLC709203Fパッケージ、その証明書で署名した証明書設定EXE。`prepare-bqhr-combined-wim.py` は入力WIM・storage SYS/INF/CAT・SHA EXEの既知SHA-256を検査し、診断markerのあるARM32署名付きSYSを要求する。PE署名欄の存在確認は暗号学的信頼の検証ではない。コピーWIMを更新し、wimlib verifyと全追加ファイルの抽出ハッシュ照合を行う。元WIMは変更しない。

```
LD_LIBRARY_PATH=/tmp/a04br3-wimtools/usr/lib/x86_64-linux-gnu \
python3 tools/winpe/prepare-bqhr-combined-wim.py \
  --wimlib /tmp/a04br3-wimtools/usr/bin/wimlib-imagex \
  --source-wim ../../port/xnu/out/windows-rt-multiblock-run3/stress.wim \
  --battery-package /media/yuma/Expansion/TAB-A04-BR3/windows-rt/windows-bqhr-combined/package \
  --storage-package ../../port/xnu/out/windows-rt-multiblock-5ec0aaa/package \
  --stress-exe ../../port/xnu/out/windows-rt-multiblock-5ec0aaa/msdc-sha256-stress-signed.exe \
  --cert-helper /media/yuma/Expansion/TAB-A04-BR3/windows-rt/windows-bqhr-combined/bqhr-test-cert-v2.exe \
  --cert-helper-sha256 a6daf71a61e4b4162e3df8a4711d24a882a3e9293b4e8ca585e92697ecfcd1e2 \
  --output /media/yuma/Expansion/TAB-A04-BR3/windows-rt/windows-bqhr-combined/combined-v2.wim
```

署名済みLC709203FはWindows 8.1 WDKで `A04_BQHR_DIAGNOSTIC=1 A04_LC709203F_TRACE=1 A04_BQ24160_EXPERIMENTAL=0` を指定してビルドし、`tools/ci/sign-lc709203f-test.ps1` で試験署名した。`tools/ci/build-bqhr-test-cert.ps1` は同梱証明書をWinPEに登録するEXEのARM32ビルド用であり、同じ開発証明書での署名は別途必要。上のhelperハッシュは今回回収したビルドに限る。非WinPE Windows上ではguardが拒否し証明書ストアが変化しないことをx64ビルドで確認した。WinPE内での証明書登録・drvloadは実機未検証。

## ESPとUEFI

`tools/winpe/replace-esp-wim.py` に保存済み `windows-rt-multiblock-run3/stress-esp.img` と生成WIMを渡す。既存BCD SHA-256 `65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d` と元WIM SHA-256 `8ebf8a4c14d16b233d6416f848a39acbedd9a72d945452bfe6442f549dc30835` を指定し、別ファイルにコピーしてWIMだけを置き換える。FATの大小文字差を解決し、画像内WIMとBCDのSHAおよび `fsck.vfat` を検査する。元ESPは変更しない。

同じ成果物ディレクトリの `boot.img` はUEFI commit `691f907`、3 CPU構成、CPU1 worker期限43200秒、BQアクセス禁止の診断ビルドである。`uefi-manifest.json` と同じディレクトリのWIM/ESP manifestにソースとハッシュを記録する。生成・ホスト検査は実機への書込み・起動を意味しない。

## thermal併用

現行thermal構成 `663fcb1` は4 CPU、GPU handoff無効、CPU 1.3 GHz無効を要求する。thermal KMDF診断はTOPRGUの `ownedFour` を必須としており、今回の3 CPU/CPU1 worker構成とは一致しない。候補thermal初期化はAUXADC・APMIXED・infra clock/resetの排他も必要で、熱保護の実動作は未検証。GPU ACPI資源がthermal資源と重ならないだけでは共有clockやwatchdogの安全を示せない。現時点で安全に併用できる条件は確認できず、このイメージにはthermalを含めない。
