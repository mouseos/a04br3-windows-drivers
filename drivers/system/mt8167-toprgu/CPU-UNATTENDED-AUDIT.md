# 無人CPU監査とDISMを使わない復元

主担当が実機を操作する。試験イメージの実機起動、監査、UART送信、UART resetとrecovery復元は未検証。搭載Windowsにはドライバ、証明書、レジストリを導入しない。監査内容はホスト側署名受理やCPU1所有の成立を証明しない。

## 入力と出自

先行成功は正本UEFI `1b705d65fecacb21848d8d8b9d77339a61708df7` boot SHA-256 `1ff6bd83afe550d5e5fa7846d36623d628d1aa8b1b58730238cd3a26cc328e80`、ESP `ed92f50a7ba5464226e9781be6613135ed02bada702d3eb23300d85b902bd26e`。今回のESPは先行ESPからWIMだけを交換した**未実機検証の新構成**。現行正本3a310cccは子孫だがコードに差があり、このbootと同一とは扱わない。`FOUR-CPU-TEST-PREP.md` に旧実績と署名の根拠がある。

成果物は `port/xnu/out/windows-rt-cpu-auto-9763689/`。`auto-esp.img.json` はESPと元画像のSHA-256、WIM boot index、他87ファイルの一致を記録。`auto.wim.json` はWIM、署名済みUART送信EXE、置換startnet、元startnet、報告識別子を記録する。署名済みTOPRGUとWinPEのテスト証明書は監査ESP `windows-rt-toprgu-audit-698dd3a` と同じ。入力のコピー以外を推測で再生成しない。

## 起動から報告まで

新startnetはwpeinit後に既存MSDC試験INFをdrvloadし、監査cmdを自動実行する。cmdはボリュームガードに成功した後に限り、対象WindowsのSYSTEM・SOFTWARE・SYSをX:へコピーし、コピーだけをレジストリにロードして選択ControlSet、サービス、SYS一致、テスト証明書キーを調べる。守れない場合は監査を拒否し、結果を送信する。NTFSの自動マウントやファイルシステムの内部動作による実際の書込み有無は未検証。入力ボリューム名・C:割当が異なればガードは拒否する。

監査終了コードが0なら `mountvol C: /p` を呼び、返り値を報告に追加する。ガードや監査失敗時は、誤ったC:をアンマウントしない。いずれもDISMは呼ばない。監査結果はTOPRGUの既存UARTキューへ、16文字の固定識別子と断片ごとのCRC32・全体CRC32を付けて送る。キュー受理だけでは送信完了ではない。UART reset受信・既存watchdog給餌処理は変更していない。

主担当は対象起動の前にUART収集ログのバイト数を `OFFSET` として保存し、他のリセット元を排他してから、指定画像と回復方法を判断する。以下は作業ルートのWindowsドライバworktreeから、収集ログと新規出力先を指定して実行する監視例。`TOKEN` は成果物の `auto.wim.json` または `token.txt` の値を使う。

```sh
python3 tools/recovery/monitor-cpu-audit.py "$UART_LOG" \
  --offset "$OFFSET" --token "$TOKEN" --output "$REPORT" --timeout 1800
```

監視は完全な一連のフレームとCRCを検証し、`TOPRGU_AUDIT_FINISHED_READ_ONLY`、`TOPRGU_AUDIT_EXIT=0`、`MOUNTVOL_EXIT=0` が各1回存在する場合に終了0を返す。監査失敗やアンマウント失敗でも完全な報告は保存し終了3を返す。欠落や時間切れではreset・復元を行わず終了する。終了0はフレームとコマンド戻り値の確認であり、割当C:に関する実機の安全性、WinPE外の処理停止、搭載OSによる署名受理やPnP・長期安定の証明ではない。監査結果には対象OSのパスや設定が含まれ得るため扱いに注意。

## UART resetとrecovery復元

報告が完全で監査・アンマウントが成功し、別のディスク更新が走っていないことを主担当が確認できる場合に限り、既存 `unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh` でUART `A04BR3!RST` を送る。受信・fastboot復帰は実機確認が必要。失敗・未完了・不明時にタイムアウトを理由にresetしない。主担当が対象機をAndroid recoveryに移し、root recoveryと保存済みの正しいboot・ESPおよびハッシュを用意した後、`tools/recovery/restore-toprgu-diagnostic.sh --verify-only` でローカル検査し、同じ引数で `--execute` すると既存のrecovery書込み経路でboot・ESPを順に戻す。詳しい引数と制限は `FOUR-CPU-TEST-PREP.md`。この処理にはDISMを使わず、自動rebootもしない。部分復元や失敗時はログを保持して停止する。

## ホスト検証

`tests/cpu_uart_sender_host_test.py` は実際のC送信コードをWin32模擬で実行し、busy再試行と受信CRCを検査。`tests/cpu_audit_uart_test.py` は欠落・破損・順序・タイムアウト・アンマウント失敗を検査。Windows runnerの `tools/ci/test-cpu-audit-startnet.ps1` は成功、ストレージ失敗、ガード拒否、アンマウント失敗を模擬。既存 `test-toprgu-installed-audit.ps1` はコピーした模擬ハイブとガードを検証。復元側は `tests/toprgu_restore_host_test.py` でハッシュ不一致やrecovery未到達で書込みへ進まないことを検証した。いずれも実機のUARTやディスク動作の証明ではない。
