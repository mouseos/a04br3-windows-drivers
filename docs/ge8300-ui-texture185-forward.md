# UI texture: batch185運用sourceへの前方結合と標準D3D評価候補

運用source基点03399df5169c8efe50b9c60857a5cb70b56b3e30。親1120/thermal1.11/MMA/clock1120/POST policy360/helper、実OS AcquirePostDisplayOwnershipのcallback経路を保持。POST fieldの実名はDisableAutoAcpiPostDeivce。185で1→0を一回変更する契約を次回に再実行せず、185後の最新freshで実値0と実AcquirePOST観測を保持する。未取得の成功を推測しない。runtimeの偽POST fallbackは追加しない。

185運用checkoutは実signed359に対し5GPU pathの古いsnapshotがあった。miniport.c/display_adapter356.c/bdd.cxx/start_gap359.hの観測とwindows_native_job.cの実359 segmented-zero確認をf4c80be6から限定復元した。これは実際に保持する359署名objectへsourceを合わせる作業。親、thermal、MMA、clock、POSTツールを旧tree全体へ戻す操作ではない。GPU sourceは359とUI9差分以外runtime byte不変。UI9は33540f92の署名済みfixtureと同一で再build不要。

新clientはd3d-ui-texture.c→既d3d-clear.cのidentity/ログ/GPR1 setupを共用し、legacy suiteをpreprocessorで除外する。IMMUTABLE InitialDataで3x5と257x129、BGRA87/RGBA28、SysMemPitch=width*4+7をupload。POINT+CLAMP、各画像の最初/最後texel中心をCB0.xyへ入れ、既finite SAMPLE PSで三角形をGPU Drawする。全4case/8 Draw。Default UpdateSubresourceUPは使わず、Carsonの独立実装と競合しない。通常Presentやvarying UV、任意DWM shaderの資格を主張しない。

実Draw→CopyResource→Flush→Map READの出力だけを判定。800x600のうち内部149784/外部322536をchannel tolerance2で確認、edge7680は除外。源画像/PPM oracleはCPUで生成する期待値でありGPU出力ではない。source guard/padding保持を別確認、COM resourceをdistinctに作りSRV/RT同一bindを避ける。このclient検査を物理PFN alias不在の証明とは呼ばない。upload sourceとCOM refsは2回のMap完了まで保持し、ClearState後Release。失敗時のinflight refsは標準runtimeの所有契約へ委ね、偽停止/手解放しない。

新exe一回のコマンドは`ge8300-d3d-ui-texture.exe --hardware=PowerVR`。外側client900秒/診断全体1500秒、実reset/bugcheckは即回収。blockingAPIに対し内部tickで停止を保証しない。ログは既固定C:\MT8167D3DClear\clear-*.log、GPR1はclear-boundary-*.logの4096B binary。UI schema1、stage40..51、detail=(case<<16)|passをevaluation-contract.jsonへ固定。GPRはstage別の最新値で全履歴ではない。新service RUN/PID、選択driver版/SYS/UMD/clienthash、最新baselineのname+hash差分を同bootで結合する。部分pass成功と最終exit/resetを分離する。

新host oracle testのみASan/UBSanで確認。旧16case/DDI15caseの成功は元receiptを参照し再試験していない。clientは既42729039661 cacheで評価TU+memory helperの2TUだけARM compile/link/BCBF sign、driver/SDK/PCO再buildなし。compile commit8c5878d8以後の変更はGPU sourceを既署名359へ合わせる限定復元とtool/provenance/docsだけで、新client compile closureは不変。署名fixtureのcompile33540f92/packaging804d48c7という出自を新commitで上書きしない。

再現はtools/ci/build-ge8300-ui-client.py。task-root/sourceへpins記載のclient closure、task-root/client-input.jsonへ実compile flagsとsource pinsを置き既Windows runnerで実行する。driver9TUの再現recipeはbuild-ge8300-ui-texture359.pyと元compiler-link-delta9library/359archive pinsを使用し、全SDK再setupしない。評価候補の正本は作業root内port/xnu/out/windows-rt-ui-texture185-forward/HANDOFF-main.md、candidate-manifest.json、pins.json。185媒体/fresh184へarmしない。次の正当な一括batchは185後最新freshのmain独立seal後にmainだけが実行する。
