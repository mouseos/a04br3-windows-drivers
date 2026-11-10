# MT8167 CPU clock read-only snapshot（parent1117基点）

GPU358/run181・現media・実機を変更しない独立source候補。新SDK、runner占有、rail/PLL/DDR書込み、fmeter/PMU、EDK変更なし。親1117の既INFRA/APMIXEDマッピングと同一mutex/rundownを使い、query interface経由でraw registerを取得する。DDR資源を増やさず、DDR Hzと実効CPU Hzは未確認を保持する。

## ソース基点と実配置版の保持

正式remoteは https://github.com/mouseos/a04br3-windows-drivers.git 。親基点b2c1a3136a5b8eadf1900f71a8d330a6b0a31ac8。親branchに残っていた古いthermal leafから再buildしないため、実配置済み1.10の署名source740800733fe9741023038026a717fd54d43cd7c3からleaf5fileだけを完全一致で採用したcommit cd37341a。その上へclockの任意差分を追加した。旧親全体・GPU/USB/audio/sourceを戻していない。thermal V3/build1104、HPv2/build1107、親13helperと既28B温度/64Bstatus ABIは保持。

MT8167_CLOCK_SNAPSHOT_QUERYは親/leafとも既定0。1で親query、既resource-less thermal PDOのSendQueryToParentStack、leafの新READ_DATA IOCTL0xa06を有効化する。他childへ新queryを広告せず、新device/service/ACPI/resource/ownerを追加しない。既0xa04温度・0xa05statusは不変。flagOFFのleafは実74080073と同じ処理であることをsource除去比較で検査する。新INF版・SYS/CAT・署名候補は作っておらず、このbranchの既1.10 INFを新releaseとして配備してはならない。

## 実取得経路

ユーザーtool `mt8167-clock-snapshot --snapshot` → 唯一の既thermal interface/実service Mt8167Thermalを確認 → METHOD_BUFFERED/FILE_READ_DATA/入力0/出力128byte → resource-less leafのPASSIVE sequential queue → Mt8167ReadParentClockSnapshot → WdfFdoQueryForInterface（新GUID/version1/resource contract1117）→ thermal PDOから親stackへ転送 → ParentClockSnapshot。

queryが取得したobject referenceは成功・失敗ともconsumerが一度返す。親は既thermalRundownを取得し、ParentLock/pwrapLockの中でD0 admission・removed・regs.online・既map/長さを確認。各read前とpublish前にもclose/removeを確認する。ParentStopはadmissionを閉じ同じrundownをdrainしてからunmapする。新しいmutexやPnP lifetime owner、MmMapIoSpace、PWRAP transaction、thermal Acquire/Measure/ATCO消費はない。物理的な突然の電源断/外部firmwareの全排他まで保証する契約ではない。

失敗時はNTSTATUS失敗でIOCTL返却byte0。成功は22回のraw read完了を示すだけで、安定・周波数・実機動作成功ではない。完全だが異なる2sweepはUNSTABLEを付けrawとして返す。partial mask、admission lost、逆時刻/all-onesは失敗。固定回数で再試行なし。既mutex待機を新たなhard deadline保証とは呼ばない。UMは2秒後cancelし1秒drainを待つが、待機timeoutを有効なsampleにしない。

## 固定read whitelist（CPU PLL/muxのみ）

|index|親bank / offset|Linux名|
|---:|---|---|
|0|INFRA +0x000|ifr_mux1_sel（bits3:2）|
|1|APMIXED +0x014|PLL_HP_CON0|
|2–4|APMIXED +0x100/+0x104/+0x110|ARMPLL CON0/CON1/PWR|
|5–7|APMIXED +0x120/+0x124/+0x130|MAINPLL CON0/CON1/PWR|
|8–10|APMIXED +0x140/+0x144/+0x150|UNIVPLL CON0/CON1/PWR|

最初はindex0→10、後半は10→0。前後KeQueryInterruptTimeの100ns bracketを付ける。値が等しくてもfirmwareが途中で往復するABA、全期間の不変性、独立参照の正確さは証明しない。任意address/offsetのIOCTLは提供しない。

CT3公開source7c4782127d4ea11965be00ce1343056be9bfb769のclk-mt8167.cはINFRA0x000/shift2/width2をclk26m_ck、armpll、univpll、mainpll_d2に対応付ける。APMIXED PLLとclkdbgの実register名を一致させた。clk-pll.cのMT8167 INTEGER_BITS=7、PCWbits21/21/7、postdiv=1<<CON1[26:24]、UNIV analogdiv2を使用する。LinuxのVCO/postdiv切上げ順序も維持する。

## decoderの境界

`python3 tools/cpu/decode-mt8167-clock-snapshot.py captured.bin` はrawだけで、Hzはnull。独立に保存した参照clockの根拠がある場合だけ `--reference-hz HZ --reference-basis 根拠` を追加する。DTSの公称26MHzを使う場合はbasisに「DTS nominal / 未校正」と明記する。計算結果は `configured_PLL_hz_from_supplied_reference` であり実測core Hzではない。preloaderやstock1GHz/1.3GHzを自動入力しない。ARM hopping bit0が有効ならARMPLL計算値も返さない。別parentの設定値をCPU実効値に代入しない。未確定のdownstream divider、lock/settle、高位firmware所有、参照校正を成功と推定しない。

DDRは親8rangeにDDRPHY/DRAMCを追加せずUNAVAILABLE。Linux `CLK_TOP_MEMPLL=800000000` はfixed-clock宣言で、実DDR値ではない。旧preloader1584MHz/training1600もこのsnapshotに混ぜない。rail設定/アナログ電圧/continuous thermal保護は取得・変更しない。

## source-only検証

```
python3 tests/mt8167_clock_snapshot_test.py --out ROOT内成果先
```

実sampler・parent include・本体から抽出したParentLock/Unlock・consumer・IOCTL handlerをNT/WDF host modelで実行。100ケース：22固定read、全11位置の不一致、各22位置のall-ones/stop/remove、閉じたD0/admission、missing map/長さ、rundown拒否、IRQL、query/ABI不一致、入力/出力長失敗とstatus/refs/lock/rundown収支。decoder321ケース、ASan/UBSan合格。synthetic binaryは`model-only-snapshot.bin`であり実Windows取得ではない。Windows kernel/PnP並行動作・実clock成功は未確認。

純C clock_snapshot.cだけをローカルclang18でthumbv7/COFF ARMNTへcompileしmetadataを確認した。WDK親/leaf/client/UMのARMcompile、link/import照合、SYS/CAT署名、実機取得は未実施。GPUrunner接続0。

## 増分compile recipe（今は実行しない）

`tools/ci/clock-snapshot-baseline-inputs.json` が既1117親/実1.10 leafの署名manifest SHAを固定する。既header closure/compilerが使える時だけ `tools/ci/compile-mt8167-clock-snapshot.ps1` を利用する。新SDK/全setup/SSH/署名/実行を行わない。既存manifestとheader hashが合わなければ停止し、guardを緩めない。

引数はParentManifest、ThermalManifest、KitInclude（既km/sharedの親）、UmInclude、WdfInclude（既kmdf/1.13）、新OutputDirectory（checkout内）。通常はrecipe JSONだけを出す。mainの都合に合わせ将来`-Compile`を明示した場合だけ変更5TU＋flagOFF比較2TUをcompileする。現在このPowerShell recipeは未実行。

- 親：wdf_parent_probe.c＋新clock_snapshot.cの2TU。他13helperは元source/include/object hash確認で保持。
- leaf：実1.10 wdf_native_thermal.c＋新parent_snapshot_client.cの2TU。他thermal/boot_record/readoutの3helperは保持。
- UM：tools/clock/mt8167-clock-snapshot.cの1TU。既8.1 kernel32/setupapi libsを使うlink/signは将来main側。
- PowerShell recipeはlink/signを実装しない。旧full build helperは新差分を知らず正しいlinkにならないため呼ばない。将来親/leafの新package版割当て、元helper/include/blob/object/hash検査、実RT81 exports、SYS/CAT/包含検証が必要。

## 将来mainでの採取手順（今回の実行指示ではない）

GPU181途中に混載しない。将来の一括tupleで上記新親/leafをmainがsealし、read-only toolを一度だけ既fresh logs directoryで`--snapshot`実行する。CREATE_NEWでmt8167-clock-snapshot-v1.binを保存、既存fileを上書きしない。返却128byte・flags/valid masks・同boot実SYS hashes/NTstatusと時刻bracketを一緒に回収する。旧leaf/旧親のNOT_SUPPORTEDをblind retryしない。

raw出力が得られても、continuous thermalとrail/mux/PLL切替・失敗rollback/高位ownerが揃うまで昇周波を行わない。clock snapshotをGPU成功やCPU性能改善の実測に格上げしない。

## 最終ABI / StopAdmissionDrain根拠

新query GUIDは9f1f506c-3a44-4d96-98e7-24c0c4e29167、INTERFACE.Version=1。ParentResourceContract=1117は既resource配置の契約tagであり、新SYSのpackage版を1117と偽るものではない。caller/providerはsizeofとversionを双方検査する。snapshot v1はlittle-endian、offset0 version/4 bytes/8 flags/12 valid_first/16 valid_second/20 count、24/32がbefore/after100ns、40 first11word、84 second11wordの128byte。flagsは1 UNSTABLE、2 READ_FAILED、4 TIME_REVERSED、8 REFERENCE_UNKNOWN、16 DDR_UNAVAILABLE、32 EFFECTIVE_CPU_UNKNOWN、64 ADMISSION_LOST。8/16/32は常に明示し、未取得Hzの成功合成を防ぐ。構造にHzや電圧の固定値を含めない。

親1117に対するruntime変更はquery登録/既thermal PDOの転送/既lock+rundownを使うread callbackだけ。新driver lifetimeの仕組みを作らず、ParentLock/Unlock、Stop、Unmap、Prepare、Release、Surprise、D0Exit、D0の9関数は1117とsource完全一致。変更は既定0で無効、既context/resource/温度ABIを拡張しない。

StopAdmissionDrainのsoftware順序は(1)InterlockedExchange(admissionClosed,1)、(2)ExWaitForRundownProtectionRelease(thermalRundown)、(3)既mutex下でowner停止、(4)ParentUnmapで他ownerのretain条件を検査、(5)MmUnmapIoSpace。snapshotは同rundownをread全区間で保持し、mutexを解放してからrundownを返す。Stopはrundown待機中にmutexを保持しないため、queryの後片付けを同lockで塞がない。

close前にrundown取得済みならStopは返却までunmapへ進まない。close後だがExWait開始前にrundownを取ったqueryも、ParentLockの前後admission検査でMMIOに入らず返す。read中のclose/removeは次readとpublish前に拒否し、callback失敗/ADMISSION_LOSTを返す。ExWait開始後は新rundown取得が失敗する。D0再開時は既sourceの再初期化＋admission確定を使う。これはsoftware mapping teardownのsource根拠で、突然の物理bus abort、全firmware排他、kernel並行実測の保証ではない。成功返却後もD0やclockを将来にわたり保持するleaseはない。

bounded decoderは固定128byte、11index、postdivider3bit、PCW21/21/7bit、参照1～UINT32_MAXだけを処理する。primary clk-mt8167.cのclk26m_ck=clk26m、mainpll_d2=mainpll/2とmux4選択も明示し、selected mux parentのconfigured値だけを返す。Linux clk-fixed-factor.cの整数除算に一致する。CPU postmux divider不明、reference精度未確認、実効CPU/DDR Hzはnullを維持する。全postdiv/PCW境界・参照境界・4mux・invalid ABIを含むdecoder321件とhost100件を検査した。


## WDK増分候補1120 / thermal1.11

新候補はparent 2026.11.10.1120とthermal 0.0.1.11。resource契約1117、query ABI1、thermalV3/build1104、HPv2/build1107は維持する。既1117/1.10 packageを上書きしない。両方のcompileでMT8167_CLOCK_SNAPSHOT_QUERY=1を追加する。UMはARM objectのみで、この工程ではparent/thermalだけをlink/signする。

実WDK環境では変更5TU+flagOFF2TUが通過。初回parentのcommon flags不足を修正し、parent/sampler/parent-OFFの3objectのみ元1117のO2、mfpu=none、mno-unaligned-access、fno-builtin等を保って再compileした。leaf2objectとUM objectは保持。13parent/3thermal helperと既kernel/WDF librariesはsource/include/hashを検証して再利用する。SDK再構築・署名key exportは不要。

再現はcompile-mt8167-clock-snapshot.ps1の固定manifest/header guardを通し、そのcompile directoriesをlink-sign-mt8167-clock1120.ps1へ渡す。checkout内out出力を使う場合、compile前にtask専用 .git/info/exclude の /out/ 行でbuild出力だけを除外する。tracked source差分や他のuntracked入力は隠さず、clean guardを維持する。scriptの新規output guardを維持し、再試行は別名出力へ行う。compile-recipe.jsonのsource hashが一致する既objectだけを使うため、非runtimeのrecipe修正ではcompileを繰返す必要はない。

imports契約は実RT8.1 ntoskrnl/hal/WdfLdrのhash/exportと旧1117/1.10 IATを照合する。thermal consumerのPASSIVE_LEVEL検査により新たに必要なhal!KeGetCurrentIrqlだけを明示追加許可し、旧IATにあったと偽らない。これは既parentも使用する実RT8.1 exportであり、未知symbolの包括許可ではない。

SYS/CAT署名とCAT所属の成功は候補作成の証拠であり、Windows上のload/query/stop並行性・実clock/DDR取得成功ではない。任意referenceを与えないdecoderではHz=nullを維持する。mainの次に必要な一括batchへ独立seal後に統合し、現freeze済媒体へ自動追加しない。
