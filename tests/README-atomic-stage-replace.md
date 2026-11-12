# WinPEのファイル公開をatomic replaceへ変更するhelper

`tools/winpe/atomic-stage-replace.c` は、コピー先を直接truncateする旧SYSTEM更新経路の代わりに、同じ親ディレクトリへ完全な新ファイルを作ってから公開する。既存targetへ書込み用handleを開かず、削除・truncate・copy fallback・自動再試行を行わない。パイプラインの置換はmain側の担当で、この変更には含めない。

## 本番の契約

本番leaf名は `atomic-stage-replace.exe`。CRTを使わないARMNT console PEで、import libraryはARMのkernel32.libだけ。実行開始時にNUL終端定数 `A04_ATOMIC_STAGE_V1:LEASE_QUERY:PRODUCTION` の内容をstdoutへ出す。stdoutの改行は別書込みで、EXE中のmarkerは直後にNULを持つ。

コピー時の引数はsource、destの2個だけ。シェル・環境変数展開は行わず、空白を含むパスは引数全体を引用する。sourceは `X:\a04br3\payload\` 配下、destは `C:\Windows\System32\` 配下に限定する。GetFullPathNameWで正規化した後にprefixを再確認し、別drive、UNC、device namespace、ADS、wildcard、DOS device名、末尾の空白やdot等を拒否する。長いパスはtemp suffix込みでMAX_PATH未満に制限する。

本番専用の照会モードは `atomic-stage-replace.exe --check-lease`。単独引数だけを許し、余計な引数や不正な引用は終了2で拒否する。コピー開始前と共通の開始判定を使い、GetTickCount64が1680000ms未満、kernel queryのdriver経過時間が1680秒未満であることを確認する。busy再試行後にもuptimeを再確認する。合格は0、uptime期限は3、kernel queryまたはその期限判定失敗は11。Windows領域のmap、path/volume検査、source/dest open、temp作成、publishは一切行わない。stdoutの本番marker出力とread-only device queryだけを行う。

このモードは、Windows領域に触れない初回資格確認WIMのstartnetから新SYSのreadinessを確認する用途。0終了はその瞬間のlease資格を示すだけで、1800秒SWRSTやRecovery復帰の成功証明ではない。照会によるlease延長、watchdog feed、reset要求は行わない。以後の待機・リセット理由確認・Recovery read-only captureはmainのfield qualification側で実施する。reference variantはこの単独引数を拒否するため、x64 referenceの成功で本番照会の成功を代用できない。

source/destの全親をOPEN_REPARSE_POINTで開き、reparseでないdirectoryであることを確認する。親handleはdelete共有を許さず、終了まで保持する。source最終要素と既存destもreparse/directoryを拒否する。destが存在しない場合は新規公開を許可する。Cは固定driveかつNTFSを必須とし、親のvolume serialとtempのvolume serialも照合する。source最大64MiBで、sourceはwrite/delete共有なしで開く。

開始前とsource検査後のコピー開始直前、およびpublish直前に、共有header `drivers/system/mt8167-toprgu/uart_screen.h` の `A04_UART_WINPE_LEASE_QUERY` を `\\.\A04BR3ScreenUart` へ送る。返却sizeがstructと一致し、magic=`0x314c5041`、seconds=1800、active=1であることを必須にする。activeはdriver側のrunning/owned4/4CPU/armed/not-expired/WDT enabled確認を意味する。helperが独自にownershipを推測することはない。

`elapsed=(elapsed_high<<32)|elapsed_low` はdriverのlease経過100ns ticksとして扱う。開始時は1680秒未満、publish時は1740秒未満を要求する。GetTickCount64の同じ上限も併用するため、driverのD0開始時刻がOS起動より遅い場合も保守的に拒否する。ERROR_BUSYだけ20msずつ待ち、再試行の時間予算は1秒。unsupported、not-ready、その他のエラーは即拒否する。成功時も判定結果を次ファイルへ使い回さない。

公開手順は次のとおり。

1. `dest + '.a04-stage.tmp'` をCREATE_NEWで作る。既存tempは内容を問わず拒否する。
2. sourceをstreamingコピーし、部分WriteFileも処理する。実コピー量が取得したsizeと一致することを確認する。
3. tempをFlushFileBuffersする。sourceとtempの位置を戻して全byteをEOFまで比較する。
4. tempをcloseし、targetの型を再確認する。publish用の時刻・kernel queryを再確認する。
5. MoveFileExWにMOVEFILE_REPLACE_EXISTINGとMOVEFILE_WRITE_THROUGHだけを指定する。COPY_ALLOWEDは使わない。

失敗時は開いているhandleを閉じるだけでtempを残す。同名の再実行で証拠を上書きしない。tempが存在しない早期拒否も正常な失敗結果である。呼出し元は非0終了を失敗として扱い、元のcopyコマンドへfallbackしてはならない。

| 終了コード | 意味 |
| --- | --- |
| 0 | MoveFileExW成功 |
| 2 | 引数、パス、reference限定引数等の拒否 |
| 3 | 開始時のuptime上限 |
| 4 | volume、親、source、既存target等の検査失敗 |
| 5 | temp作成・同volume検査失敗（既存tempを含む） |
| 6 | コピー、size、FlushFileBuffers失敗 |
| 7 | 再読比較・seek・temp close失敗 |
| 8 | publish直前のuptime上限 |
| 9 | publish前のtarget再検査失敗 |
| 10 | MoveFileExW失敗 |
| 11 | 開始前のkernel lease queryまたはその期限判定失敗 |
| 12 | publish前のkernel lease queryまたはその期限判定失敗 |
| 90 | referenceだけのpublish前ExitProcess fault |

## ビルドとWindows reference試験

`tools/ci/build-atomic-stage-replace.ps1` は既存clang/lld-link、WDK 8.1 header cache、CRT header cache、指定済みのWindows Kits 10 kernel32.libを使う。SDK、GPU、driverをビルドしない。署名も行わない。

必須パラメータはOutputDirectory、WdkRoot、CrtIncludeRoot。OutputDirectoryはcheckoutの `out\` 配下の新規ディレクトリに限定し、既存出力を拒否する。WdkRootには `Include\um`、`Include\shared` を持つcacheのrootを指定する。CrtIncludeRootはstdint.hを含む必要なCRT header cacheで、WDKの `Include\km\crt` を指定しない。追加includeはum/sharedとこのCRT-rootだけに限定し、intrin.hはClangのresource headerを使用する。ユーザーによるnative環境の診断では、WDK8.1のkm/crtを追加するとintrin.hの__ldrexd型がClang20 builtinと衝突し、同includeを外した既署名RX12 recipeは成功している。これはinclude構成の問題で、source/runtime不具合の証拠として扱わない。ライブラリの既定値は以下。

- ARM: `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\arm\kernel32.lib`
- x64: `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\x64\kernel32.lib`

builderは本番ARMNTと `atomic-stage-replace-reference-x64.exe` を生成し、PE machine、kernel32.dllのみのimport、markerの分離を調べる。その後 `tests/atomic_stage_replace_reference_test.ps1` を自動実行する。本番ARMNTをx64 runner上で実行しない。

referenceは `A04_ATOMIC_REPLACE_REFERENCE=1` でのみ有効になり、実使用markerは `A04_ATOMIC_STAGE_V1:TESTCLOCK:REFERENCE`。source/destとも固定の `C:\mt8167-resume235-native-43253681009\atomic-reference\` 配下だけに限定し、kernel queryとuptimeをtestclock値に置換する。引数形式はsource、dest、`--testclock`、開始ms、publish前ms、任意の `--fault-before-publish`。clockは非負DWORD範囲でpublish値が開始値以上を必須とする。faultはflush・比較・close・期限判定の後、MoveFileExWの直前にExitProcess(90)する。本番バイナリにはこれらのオプションを含めない。

専用PSは固定rootの `cases` が既にあると実行を拒否する。caseやtempを削除せず、検査後の証拠を残す。builderを再度動かす場合も既存caseを自動削除して通さない。既存target置換、新規target、空ファイル、fault後のold不変、既存temp不変、source欠損、期限境界、余計な引数、ADS、prefix逸脱、directory、親junction、size超過、ロックされたtargetを検査する。成功時は新bytesとtemp消失、遅い拒否時はold不変とflushed temp残存、早期拒否時はtemp欠損を確認する。

結果はbuilder出力内の `reference-result.json` とcase別stdoutへ保存する。結果は実行したx64 referenceの実byte SHA256と本番SHA256へbindする。build manifestは `ATOMIC_STAGE_REPLACE_BUILD_V1`、success bool、source_commit、production/referenceのname/path/bytes/sha256、production_reference_mode=false、deadline_query_required=true、source_files、stepsを持つ。production leafはmanifest親直下。reference_testsはstatus='passed'、reference_sha256、cases件数、result_sha256とresult_pathを記録する。どこかで失敗すればsuccess=falseで、試験結果なしを合格扱いしない。作業treeがdirtyなら出力directory作成前に拒否し、source_dirty=falseを必須とする。実source/headerのhashも記録し、成功前に再照合する。

compile/linkのRunは、PS5.1でnative stderrがErrorActionPreference=Stopにより最初の行で例外化する問題を避けるため、Run中だけContinueへ変更する。stderrとstdoutをUTF8ログへ統合し、直後のLASTEXITCODEを明示保存する。finallyで元のErrorActionPreferenceを復元し、ログhashとstepを保存する。起動失敗等で終了コードが得られない場合も成功扱いしない。

## R2への改善と残る限界

旧SYSTEMへの直接copyをこのhelperへ置換すれば、publish前に停止した場合は旧targetを維持し、新bytesをtempとして回収できる。NTFS内のrenameによる単一ファイル公開へ変更することで、通常の途中停止によるtarget truncate経路を除く。

ただし、MoveFileExW成功は実機の電源断耐性、ストレージcacheやmediaの永続性、NTFS破損がないことの実証ではない。FlushFileBuffers後の比較も、OS cacheを超えた物理media readbackではない。複数ファイルとSYSTEMの更新全体はtransactionにならず、publish後の新旧混在もありうる。rename前の時刻検査からrename完了までを原子的に期限へ結び付けることはできず、同期DeviceIoControlやI/Oそのもののhangを1秒で打ち切る機能もない。1秒制限はERROR_BUSY応答後の再試行だけに適用される。

temp closeからrenameまでの間に別プロセスが同じdirectoryを書き換える敵対的競合への完全な防御ではない。親handle保持は親のrenameを抑えるがdirectory内の全書込みを排他化しない。WinPEの専用stagingを他writerから隔離することが前提。targetを他プロセスが保持してrenameを許可しなければ、破壊せず失敗する。

RESET後はhost側のRecovery read-only captureに限定し、通常OSの自動起動を禁止する。partialfailは回収・検証後にだけ次engineへ進める。このhost制御はmainの統合範囲であり、本helper単体では提供しない。referenceで証明できるのはWindows上の限定されたfile API挙動と拒否条件で、実WinPE driver query、1800秒WDT、ARMNT実行、実機電源断からの復旧成功を証明しない。

実装時のローカル検査では、両PSをPowerShell parserで構文検査し、Cの両variantをcached MinGW headerとclangのx86_64-w64-windows-gnu targetでLLVM IRへコンパイルした。IRの外部宣言にCRT関数がないこと、本番/reference markerとDeviceIoControl経路の分離を確認した。これはWindows WDKでのARMNT/x64 PEビルドやWindows reference実行の代替ではない。実装担当はVM・実機操作を行っておらず、builderの実行とreference receiptの取得はmain側に残る。
