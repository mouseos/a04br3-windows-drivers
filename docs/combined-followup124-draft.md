# fresh124 共通一括profile（未発火）

fresh124 manifest aef54b0e2d3d70c083626c76d1c18cfe16b1eff8ecd6237233e83e257bd801da、Recovery boot 0adf9f8e-8773-47b3-8d2d-e8440426f9e7 のみ。旧fresh122を再利用しない。新SCM/dataはMT8167CombinedFollowup124。現GPU3.25と旧GPUサービスStart4/121partial ledgerを保持し、新327/client/fullsourceのpin到着前にGPUを再実行しない。実機owner main。

keypad1.2 source7bb166c7、LC1.7、AFE1027、親1104+thermal1.8の対応組、rotation job fix、GPU327を1回のDIF batch/予定restart1へまとめる。親1104はthermal149e6b61とACCDET7f7ce587のsource deltaだけを統合し、V3 ParentBuildも親子1104へ合わせる。旧1103二種/旧thermal1.7の混在禁止。matched UEFI DSB boot7a105を最初通常boot前に1回適用し以後維持、ready0成功化・GPIO未知writeなし。

本commitは現時点のfail-closed draft入力検証とkeypad初回install/読み取り専用HID topology、nativeサービス保持の実装。GPU327追加UMD/FW member・CAT検証、周辺結果+volume flush→最後GPU suiteを実装済み。追加候補ごとに媒体を作らず、最終pinが揃ってからSCM/native/mediaを一度生成する。現在--finalizeは実battery witnessと候補pinが揃わない限り拒否し、preview headerを署名/arm/実行しない。

fresh124ではMT6392\Buttons\3&7ce9159&0&0はConfigFlags64、HardwareID一個、Service/ClassGUID/Driver無し、Mt6392Keys登録無し。既存0.0.1.0置換と偽らない。生成FIRST_INSTALL_MASKのbit7だけがこのexact shapeで初回install可能。未知ID/既存service/存在する新SYS/異なるconfigを拒否する。classを手で生成せず標準DIF dispatcherの選択を検証する。新HID interfaceは固定Buttons hostのCM ancestryに属するdevnodeのみ、4件/Col04+kbdhid+Keyboard classを観測。ReadFile/入力注入/物理操作要求なし。これだけで実キーやStart成功とは称さない。

LCは実選択0.0.1.0/oem53.inf。現lc709203f.sysがfresh124 28fileにないため、同Recovery boot/fresh hashへ結合したreadonly補助manifestの実byte pinが必要。旧raw値/不在を成功にしない。補助fileが来るまではDRAFT/normal_install_allowed false。

旧7service（EngineProbe/RenderSingleGpu/RenderStage/DeviceInstall/RpcSs/PeripheralBatch/PeripheralNext122）をbounded subtree fingerprintで維持。新service以外をarmしない。元trioはloadせずprivate cloneのみ、正常unload/権限復元/原hash不変を要求。SOFTWARE/userfs/GPT rollback無し。

WiFi1012/run77は標準scan完了/BSS3/全3timestamp更新の過去実績あり。1020はそのsourceを継承するが、現1020のRF/RSSIは未観測。後段のscanはexact MT6630 interfaceへ1回、30秒bounded通知と前後BSS/timestamp/非secret RSSIだけ。接続/profile作成/SSID/BSSID/鍵出力なし。受付のみをRF成功と言わない。

最終SCM builderはtools/ci/build-combined124-service.ps1（Serviceだけ、1TU）。既存WLAN/driver/SDK再buildなし。媒体はtools/winpe/build-private-peripheral-batch-media.py --combined124-profile。旧rotationの3EXEを旧媒体の固定byte witnessと実機で事前比較し、旧logをrun123-preserved名へmove後再比較する。archive既存/元log不一致なら書込み前拒否。新wrapperのみ上書き許可、原log削除なし。LC実SYSと旧UMDをstage baseline比較する。

install phase900秒、postreboot diagnostic phase1500秒、GPU client900秒、rotation外側300秒。SCMは周辺結果file/registry/volume flush後GPUへ。GPU client ID5、service RUN/PIDはPB0x286/287、作成PID0x220、client結果0x221。各phase終了はcombined_finalize→PB0x288(finalization rc)→PB0x207(terminal rc)。main controllerは同phase finalization0を正常自動回収条件とし、未証明terminalはmain review、実crash/resetは別回収分岐。0x10b/3010予定restart契約は不変。

専用post reader tools/recovery/read-combined124-post.ps1はfresh126原trioの新cloneだけを読み、13fixed nodes/旧7SCM/旧GPU Start4/新CombinedResult4phase/FactoryCheckpointを記録。Complete1は観測snapshot完了であり機能成功を意味しない。元triohash不変、load/unload/restore0、未知型・未知combined schema拒否を要求。
