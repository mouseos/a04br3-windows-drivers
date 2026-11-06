# 搭載WindowsのTOPRGUと4 CPUの診断

現在のWinPEではCPU1のWindows側への引継ぎを観測した。搭載Windowsでは
SYSの候補との不一致を観測したが、SYSTEMハイブの読込みが失敗しており、
boot-start設定、署名の信頼、PnP開始はいずれも未確認である。C:の解放も
未確認である。`startnet-upgrade-toprgu.cmd` は安全な更新手順ではなく、
意図的に無効化されている。

正常なdismountの証拠が得られるまで、現在のWinPEには介入しない。後続の
保守起動では、対象ボリュームを識別してから `audit-toprgu-installed.cmd`
を実行し、コピーしたSYSTEMとSOFTWAREのロード、選択ControlSet、SYS比較、
証明書を確認する。終了成否にかかわらずロード済みハイブの有無を確認し、
`release-toprgu-audit-volume.cmd` の `TOPRGU_VOLUME_RELEASED` を確認する。
監査失敗、hive unload失敗またはdismount失敗は更新可能の証拠ではない。
証明書がストアにあることやファイル署名の存在は、実機のコード完全性
ポリシーによる受理を証明しない。

実機の監査と正常dismountが得られ、搭載Windowsが起動可能になった後、
同じ起動中に管理者コンソールから `verify-toprgu-installed.cmd` を実行する。
同じディレクトリにWDK 8.1 ARM32ビルドの `toprgu-provider-status.exe` を
配置し、出力全体と同時刻のUARTを保存する。サービス設定・実行状態、
PnP状態、CPU列挙、利用可能な場合のCPU使用率をまとめて記録する。
終了マーカーは収集終了を示すだけであり、個々のコマンド結果を確認する。
CPU数はownership、各CPUでの処理、継続給餌、性能または安定性の証拠ではない。
カウンター名がローカライズされていればサンプルは失敗し得る。

4 CPUの判断には同一起動のUEFI設定・出自、引継ぎと給餌のUART、搭載OSの
ドライバ受理・PnP状態、全CPUの実行、十分な負荷と経過時間、再起動原因を
別々に確認する。比較可能な負荷と測定条件がないCPU使用率を性能改善と
扱わない。スリープ・復帰も別途未検証である。

搭載OSの実態が不明な現段階では、署名・信頼・元ファイルとレジストリの
保存・復旧・電源断時の整合性を確認できる更新トランザクションは未完成。
この診断は更新イメージでも、4 CPUを有効化する操作でもない。
