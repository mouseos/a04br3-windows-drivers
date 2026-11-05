# 4 CPU TOPRGU 診断と搭載Windows監査の準備

この資料は端末を書き換えずに作成した。診断ESP、WIM、テスト署名、後述の復元プログラムはいずれも今回の実機で未検証。搭載Windowsには新ドライバも証明書も導入していない。主担当が並行作業とリセット元の排他を管理する。

## 固定した先行実績と今回の差

正本UEFI `1b705d65fecacb21848d8d8b9d77339a61708df7` のboot SHA-256 `1ff6bd83afe550d5e5fa7846d36623d628d1aa8b1b58730238cd3a26cc328e80` と、WinPE ESP `ed92f50a7ba5464226e9781be6613135ed02bada702d3eb23300d85b902bd26e` では、同一起動でCPU1所有確認と給餌、画面転送が観測された。`port/gsi/logs/ios-feasibility/windows-rt-canonical-1b705d65-run1/observations.md` とUARTが根拠。WIMはBoot Index 1、既存SYS SHA-256 `52a1a1cb0cd66821099a87db9aca5bd82797061ab0e99a047d495d7fe06c2b3e`。

現行正本 `3a310ccc420d2c76213733ff7577517e0bb155b8` は上記コミットの子孫だが、GPU ACPI、eMMCおよびDSC等に差分がある。今回の新ESPを現行正本の別bootや前回のUEFIガード実験bootと組み合わせた実績はない。既知の1b705d65 bootと今回の新ESPも、WIM内のSYSと監査ファイルが変わるため**新規の組合せ**である。これを過去の成功と扱わない。

搭載Windowsの別ログには旧 `D0 active CPUs>=4` と給餌があり、別の監視記録はD0後660秒間WDT再起動なしと報告する。新しいCPU1確認版が搭載されていた証明や、現行搭載SYS・BCD・証明書の証明ではない。詳細は `INSTALLED-FOUR-CPU.md`。

## 成果物と検証

`port/xnu/out/windows-rt-toprgu-audit-698dd3a/` の監査ESP SHA-256 `cce90b20d383e83111e0dc9d9a02865b7332b2cd98cd8f3ab9e920981083e39f` は先行ESPのコピーからWIMのみ差し替えた。WIM SHA-256 `926dda50a6b06b5eb0d7d494460e7e19e6583746144e01cb50ca33ec971b35cd`、1 image、Boot Index 1。他87ファイルはハッシュ一致し、FAT検査済み。先行ESP、BCD、boot.sdi、bootarm.efiは保管された元を変更していない。

新しいSYSは `port/xnu/out/windows-rt-toprgu-winpe-7109a90/signed/`、SHA-256 `9ca96c1f54964a132578a00e2362795c789ac627d4e925df69aa8bc17b28e7fa`、CPU1 handoff=1、UART screen=1、thermal stop=0、deadline=0。Windows 8.1 WDK ARM32でビルドしテスト署名した。証明書のSHA-1 thumbprintは `5FC3D0043DE4A471F738FB923FEC87336E81D43C`。一時Windowsランナーにテスト証明書を信頼させ、SYS署名、CAT署名、CATへのSYS登録をsigntoolで検証。署名済みSYSとStart=0の選択をオフラインWIMの再マウントで確認。これは対象RTでの署名受理・PnP開始を示さない。証明書の秘密鍵は成果物に含まない。

再利用したランナーのビルドディレクトリに前回の画面機能無効・未署名manifestが残っており、生回収ZIPにも混入していた。履歴として `raw-runner-package-with-stale-metadata.zip` と `prior-unsigned-manifest-not-candidate.json` に区別して保持する。候補の配布物はSYS等と正しいmanifestだけを収め、各ハッシュを照合した `test-signed-package.zip` を使う。再現スクリプトは混入manifestを署名パッケージから除くよう修正した。

生成手順: `tools/ci/prepare-toprgu-fourcpu-winpe.ps1`、`tools/ci/prepare-toprgu-audit-wim.ps1`、`tools/winpe/replace-diagnostic-wim.py`。ソース・入力・中間・出力ハッシュと設定は各成果物のJSONに記録。処理ログは `port/gsi/logs/ios-feasibility/windows-rt-toprgu-*7109a90*.log` と `windows-rt-toprgu-audit-*`。テスト署名を再生成すると鍵とハッシュが変わる。監査スクリプトのthumbprintは上記証明書に固定しており、別証明書へ差し替える場合はスクリプト・テスト・manifestも更新する。

## 主担当による監査と判定

1. 現在のGPU作業を終え、既存boot・ESPの出自とハッシュ、復旧可能な回復環境、対象シリアル、他の自動リセットの停止状態を記録する。保存済み復元画像を主担当が特定できない場合は試験しない。
2. 固定済み1b705d65 bootと今回の監査ESPを使う場合は新しい組合せと明記し、対象WinPEがディスクservicingを実行していないことと復旧条件を確保する。`run-display-winpe-test.sh` の固定時間復元は使わない。DISM中にタイムアウトによるreset・復元を行わない。
3. 選択した起動のUART開始オフセットを保持し、`python3 tools/recovery/analyze-toprgu-handoff.py UART_LOG --offset OFFSET` でExitBootServicesの印、CPU1確認、以後の複数kick、後続BOOT_REASONを解析する。ログ欠落や再起動境界に注意。これはWinPE上の観測であり署名、搭載OS、長期安定の証明ではない。
4. 監査ESPにある `X:\a04br3\audit-toprgu-installed.cmd` は対象C:がすでに正しく割り当てられている場合にのみ実行する。必要なストレージdrvload、ドライブ選択、既存データの保護は主担当が判断し、同梱 `guard-windows-volume.exe` の成功を前提にする。スクリプトはC:のSYSTEM、SOFTWARE、SYSをX:の新規ディレクトリへコピーし、コピーだけをハイブとして読み、選択ControlSet・Start・Group・ImagePath、候補SYSとの一致、テスト証明書のRoot/TrustedPublisher存在を報告する。既存のX:監査ディレクトリや一時ハイブ、ボリュームガード失敗時は拒否する。RAM上の結果は電源断で消える。証明書のキー存在だけでは署名の検証やOSによる受理を証明しない。
5. 搭載OSでの確認は別起動としてUARTを区切り、そのOSボリュームと実際のSYS・CAT、署名ポリシー、署名受理、PnPデバイスのD0とCPU1取得・継続給餌、後続起動理由を照合する。WinPEの `CPU1 ownership acquired` を搭載OSの証拠に流用しない。

現在、搭載OSの候補証明書受理、現行SYSの完全な出自、両ハイブとドライバストアの安全なバックアップ／ロールバック、DISM完了と確実なアンマウント、無期限のwatchdog保持は未確立。したがって搭載Windows向けDISM・証明書導入を開始する条件は満たしていない。既存 `run-mma8652-offline-stage.sh` は正本の無期限servicing未検証のため無条件拒否のまま。期限や単発画面表示を根拠にDISM中の復元を行わない。

## 回復環境からの自動復元

`tools/recovery/restore-toprgu-diagnostic.sh` は、主担当が**すでにAndroid recoveryへ移した後**に、指定した以前のboot・ESPのサイズ、SHA-256、対象シリアル、root recovery、パーティション位置、非マウント、転送後ハッシュを検証し、bootとESPを順に戻す。予め `--verify-only` でローカル入力を検査でき、このモードは端末へ接続しない。`--execute` の実行ログ保存先は作業ルート内の未作成ディレクトリにする。書込みの通常読戻しはしない。途中失敗は部分復元として停止し、reset・reboot・時間切れ復元は一切行わない。`RESTORE_COMPLETE_NO_REBOOT` は両書込み・syncコマンドの報告であり、端末の次回起動成功の証明ではない。

```sh
tools/recovery/restore-toprgu-diagnostic.sh --verify-only SERIAL SAVED_BOOT BOOT_SHA256 SAVED_ESP ESP_SHA256 NEW_LOG_DIR
tools/recovery/restore-toprgu-diagnostic.sh --execute SERIAL SAVED_BOOT BOOT_SHA256 SAVED_ESP ESP_SHA256 NEW_LOG_DIR
```

引数は主担当が作業前に実際の保存済みファイルとハッシュから指定する。recovery移行前のDISM停止・完了はこのスクリプトには判断できず、外部リセットやデバイスマッパー経由のマウント排他も保証しない。未検証の新規スクリプトであり実行例を実機成功として扱わない。ホスト模擬ではハッシュ不一致、recovery判定失敗、幾何検査マーカー欠落で書込みへ進まないことと、成功経路の順序・reboot不実行を検査した。
