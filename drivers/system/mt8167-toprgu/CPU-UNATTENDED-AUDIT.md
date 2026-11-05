# 無人CPU監査とDISMを使わない復元

主担当が実機を操作する。試験イメージの実機起動、監査、UART送信、UART resetとrecovery復元は未検証。搭載Windowsにはドライバ、証明書、レジストリを導入しない。監査内容はホスト側署名受理やCPU1所有の成立を証明しない。

## 入力と出自

先行成功は正本UEFI `1b705d65fecacb21848d8d8b9d77339a61708df7` boot SHA-256 `1ff6bd83afe550d5e5fa7846d36623d628d1aa8b1b58730238cd3a26cc328e80`、ESP `ed92f50a7ba5464226e9781be6613135ed02bada702d3eb23300d85b902bd26e`。今回のESPは先行ESPからWIMだけを交換した**未実機検証の新構成**。現行正本3a310cccは子孫だがコードに差があり、このbootと同一とは扱わない。`FOUR-CPU-TEST-PREP.md` に旧実績と署名の根拠がある。

修正版成果物は `port/xnu/out/windows-rt-cpu-release-ac1072b/`。ESP SHA-256 `5a5917dc81c8e6b68ce4e7db56d26dc442cc76c5e5c96e411c0df55a9b6d17ac`、報告識別子 `AF205419D7D71058`。`auto-esp.img.json` は元画像、WIM boot index、他87ファイルの一致を記録する。`auto.wim.json` は元WIM、置換した3つのcmd、報告識別子を記録。署名済みUART送信EXE・TOPRGUと元startnetは修正前WIMと一致する。元の `windows-rt-cpu-auto-9763689/` は今回の失敗を観測した旧版として保持する。入力のコピー以外を推測で再生成しない。

## 起動から報告まで

新startnetはwpeinit後に既存MSDC試験INFをdrvloadし、監査cmdを自動実行する。cmdはボリュームガードに成功した後に限り、対象WindowsのSYSTEM・SOFTWARE・SYSをX:へコピーし、コピーだけをレジストリにロードして選択ControlSet、サービス、SYS一致、テスト証明書キーを調べる。守れない場合は監査を拒否し、結果を送信する。NTFSの自動マウントやファイルシステムの内部動作による実際の書込み有無は未検証。入力ボリューム名・C:割当が異なればガードは拒否する。

監査終了コードが0か、初回ガード成功後の監査失敗2の場合、別のrelease cmdがガードを再確認し、HKLM直下を列挙して両一時ハイブ名が存在しない場合だけ `mountvol C: /p` を試みる。初回ガード拒否4、一時ハイブのunload失敗3、再ガード拒否、レジストリの列挙失敗、ハイブ残存時はアンマウントしない。`MOUNTVOL_EXIT`、`TOPRGU_VOLUME_RELEASED`、`TOPRGU_RELEASE_EXIT` は実行経路に応じて報告する。これはコマンド結果であり、他のプロセスや他ホストがI/Oしないことは保証しない。DISMは呼ばない。監査結果はTOPRGUの既存UARTキューへ、16文字の固定識別子と断片ごとのCRC32・全体CRC32を付けて送る。キュー受理だけでは送信完了ではない。UART reset受信・既存watchdog給餌処理は変更していない。

主担当は対象起動の前にUART収集ログのバイト数を `OFFSET` として保存し、他のリセット元を排他してから、指定画像と回復方法を判断する。以下は作業ルートのWindowsドライバworktreeから、収集ログと新規出力先を指定して実行する監視例。`TOKEN` は成果物の `auto.wim.json` または `token.txt` の値を使う。

```sh
python3 tools/recovery/monitor-cpu-audit.py "$UART_LOG" \
  --offset "$OFFSET" --token "$TOKEN" --output "$REPORT" --timeout 1800
```

監視は完全な一連のフレームとCRCを検証し、監査完了・監査終了0と、`MOUNTVOL_EXIT=0`、`TOPRGU_VOLUME_RELEASED`、`TOPRGU_RELEASE_EXIT=0` が各1回存在する場合に終了0を返す。監査が失敗してもreleaseの3条件が揃えば終了5、release失敗や不明なら終了3を返す。欠落や時間切れではreset・復元を行わず終了する。終了0や5はフレームとコマンド戻り値の確認であり、WinPE外の処理停止、搭載OSによる署名受理やPnP・長期安定の証明ではない。監査結果には対象OSのパスや設定が含まれ得るため扱いに注意。

## 実機のハイブ読込み失敗への修正

ユーザー申告の今回の実機reportはガード成功、SYS比較NOの後にレジストリのcorruptエラーと監査終了2。旧startnetはその場合にアンマウントせず、現時点のWinPEは旧コードのままである。UART入力はreset要求だけで、既存WinPEへ新しいrelease cmdを遠隔注入して実行する経路は確認できていない。新ESPを作っただけで今のC:が解放されたとは扱わず、現在のWinPEをタイムアウトでresetしない。

旧監査はSYSTEMとSOFTWARE本体だけをX:へコピーしており、トランザクションログをコピーしていなかった。以前の別起動で取得したSYSTEMはヘッダーシーケンス804/803、LOG1とLOG2のオフライン再生で35ページを回復できた。これは今回の失敗がログ不足だった可能性を支持するが、現在のハイブ・ログを採取しておらず原因の確定ではない。`TOPRGU_SYS_MATCH=NO` も比較ツールの不一致と読取りエラーを区別できない。

新監査は各ハイブ本体と存在する `.LOG`、`.LOG1`、`.LOG2` をX:へコピーして各ファイルを照合してからコピーをロードする。複数ファイルのコピーは原子的スナップショットではなく、ログが存在しない・破損・更新中・WinPEが再生を拒否する場合の成功は保証しない。コピー・ロードの成否を問わず、上記ガードと一時ハイブ確認を満たした場合に限ってreleaseする。Windows上で新しいbatchの実行は未検証。

## UART resetとrecovery復元

報告が完全で監査・アンマウントが成功し、別のディスク更新が走っていないことを主担当が確認できる場合に限り、既存 `unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh` でUART `A04BR3!RST` を送る。受信・fastboot復帰は実機確認が必要。失敗・未完了・不明時にタイムアウトを理由にresetしない。主担当が対象機をAndroid recoveryに移し、root recoveryと保存済みの正しいboot・ESPおよびハッシュを用意した後、`tools/recovery/restore-toprgu-diagnostic.sh --verify-only` でローカル検査し、同じ引数で `--execute` すると既存のrecovery書込み経路でboot・ESPを順に戻す。詳しい引数と制限は `FOUR-CPU-TEST-PREP.md`。この処理にはDISMを使わず、自動rebootもしない。部分復元や失敗時はログを保持して停止する。

## ホスト検証

`tests/cpu_uart_sender_host_test.py` は実際のC送信コードをWin32模擬で実行し、busy再試行と受信CRCを検査。`tests/cpu_audit_uart_test.py` は欠落・破損・順序・タイムアウト・アンマウント失敗・監査失敗後のrelease報告を検査。`tests/cpu_release_contract_test.py` はbatchの分岐と順序を静的に検査する。従来版はWindows runnerでstartnetと模擬ハイブを検証したが、今回の新しいログコピーとrelease batchはWindows上で未実行。復元側は `tests/toprgu_restore_host_test.py` でハッシュ不一致やrecovery未到達で書込みへ進まないことを検証した。いずれも実機のUARTやディスク動作の証明ではない。
