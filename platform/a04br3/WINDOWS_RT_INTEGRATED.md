# TAB-A04-BR3 Windows RT統合ソース

run25の主担当実測：通常WindowsのDODが実フレームをGE8300へ転送し、全1,024,000画素一致を3回、B1=0・FWstart=0・380,000,062 Hzを確認。3D・完全render UMD・長期安定は未完了。実機生ログと私用hiveは公開しない。

installer実ログもUPDATE_API_SUCCESS=1、REBOOT_REQUIRED=0、CM_PROBLEM=0、RESULT=BOUND_STARTED、FINAL_WIN32_ERROR=0、SCM_EXIT=0。最終UARTだけCreateFile error2（通常OSのScreenUart名不存在）で未達。probeも通知成功を前提にせず、diskログ・終了コードと分けて判定する。

独立branch `work/windows-rt-integrated-20261107` はGPU/MMA8652 `d5f676d` と、
USB/audio/Wi-Fi単一parent `f17b743` を履歴を保持して統合する。
KS clientの9 GUID修正はHerschel `a016d53` と同内容の `f035c59` を含む。
正確なsource pinは `windows-rt-integrated-sources.json` とmerge commitの親に記録する。
main checkoutの未コミットファイルと `.task-tmp` は利用しない。

Makefileの末尾追加競合は両方のtarget/recipeを保持して解消した。
GPU/MMA8652はGPU基点、parent/audio/USB/Wi-Fi/KS clientはperipheral基点の
実装を変更せず保持する。新しいMMIO操作、ACPI変更、EDK2変更は追加しない。

## 音声モードとGPU主試験の基点

この統合branchの音声は**固定tone診断版**（`private_fixed_tone_diagnostic`）。
主担当が回収したSYS=`f17b743`、KS client=`a016d53`も同モードであり、
GPU主試験はこの現診断parent版のまま先行する。回収済みSYSをEnginePcm版と呼ばない。

AudioSrv実PCM候補 `49d5c65` と後続 `329413a` を既定OFFの別profileとしてsource統合した。
Herschelの `work/rt-audio-retained-restart-20261107` が出自で、新しい親/audio/endpoint構成を使う。
同allocationのSTOP→RUN・PAUSE→RUN・再openはソースとホストfixtureで確認されているが、
WDK・AudioSrv列挙・実PCM更新・可聴再生・実電源遷移は未確認。
親interfaceが同じv11でも固定tone親との互換性の証拠にはならない。

**EnginePcmを使う構成にはhardwareprobe `94e4d47`を自動登録しない。**
このserviceは旧private-tone KS clientを自動起動し、保持する初回RUNを消費する。
EnginePcmではTopology追加でAUDIO interfaceも複数になるため、旧clientによる
自動列挙・起動を再利用しない。既存登録を引き継いだ構成でも、自動起動を有効なまま
EnginePcmへ切り替えない。EnginePcm専用の起動・観測方法が確定するまでは別構成として扱う。

下記のservice bundleとRecovery媒体builderは固定tone診断構成用。
実行・登録をこのsource統合が行ったという意味ではない。

## 固定commitからのビルド

clean checkoutで同じ統合commitを指定し、既存の各WDK8.1入口を使用する。
USB/audio/Wi-Fiは次で単一BEN0001 parentとKS clientを生成する。

```powershell
.\tools\ci\build-mt8167-first-playback.ps1 -IntegratedUsb -IntegratedWifi -OutputDirectory .\build\integrated-peripherals
```

追加のmanifest整合検査が必要なら `build-mt8167-peripherals.ps1` が同じ入口を呼ぶ。
GPU/DODは `tools/ci/build-wddm-display-arm.ps1` と主担当が確定したGPU feature設定、
firmware providerは `build-ge8300-provider.ps1`、MMA8652は `build-mma8652.ps1` を使う。
この統合で主担当のGPU設定を推測して別profileへ変えない。各入口の既存設定を保持する。
単一parentを別の診断専用parentで上書きしない。

peripheral同梱4 INFは11/06/2026,2026.11.6.1001。
GPU/MMA8652のINF日付・版・実ハード名はGPU基点を保持する。
実際の署名/Inf2Cat/配置/通常OS PnPと実キー入力・Wi-Fi接続・可聴再生は主担当工程。
主担当報告のf17b743全SYS WDK link成功はこの新統合commitのbuild成功を意味しない。
統合commitのWDK実行・実機/SSH/runner操作は本作業では実施していない。

## GPU追補と現在回収中のバイナリ

GPU `7d57022` のprivate output初期値A5A5A5A5（3行）を追加取り込み。
黒clear未実行なのにzero初期値との画素一致で成功と誤認することを防ぐ。
GPUのその他のソースはd5f676dのまま。

主担当申告で回収したperipheral SYSは固定tone診断版f17b743、
client=a016d53の混在で、全SYSの再compileはしていない。
この既存バイナリの出自と、新統合branchから将来buildするソースを区別する。
具体的なSYS/EXE/CAT hashと署名結果は主担当の回収manifestが正本。

## 一括観測service・Recovery更新媒体・eMMC候補

Nashのhardware probe service/probe一式は94e4d47（2f613e6のSCM契約とUART literal修正を含む）。
`build-mt8167-peripherals.ps1`は同一sourceのWLAN/KS clientをservice bundleへ渡し、
`hardware-probe`とWLAN launcher/probe manifestを一括manifestへ含める。
既存単一parentのSharedParentDirectory再利用・DriverVer・ABI検査は保持する。
serviceの署名buildは主担当が94e4d47固定で実施中という申告を記録する。

main ba26a34の `tools/winpe/build-mt8167-shared-io-update-media.py` とCMD templateを保持。
Recoveryから退避したhost hive backupの状態・byte数・SHA256・必要hive/logを確認し、
Recovery boot_idとbackup manifest hashを生成manifestへ固定する。
parent/audio/USB/Wi-Fiの4 driverを一括DISMする媒体builderで、storage driver更新は含めない。
媒体生成やDISMの実行はこのsource統合では行っていない。

eMMC d4d8d57と前提6b5043eは将来の独立候補として収録し、実機非適用。
通常sourceのA04_DMA_WRITE_MULTIBLOCK_EXPERIMENTは既定0で、明示環境flag=1以外では
汎用build入口へdefineを追加しない。既存のread-multiblockのみの構成を勝手にwrite batchingへ変えない。
専用 `build-msdc-boot-write-performance.ps1` の呼出し自体はperformance構成を明示選択するため、
この統合のperipheral/GPU/service/media入口から自動で呼ばない。
専用入口は `-SingleSectorBaseline` で比較用のread/write batchingを0にできる。
当該performance package/ファイルverifierの生成・配置・実行は主担当の別工程。

## 任意EnginePcmの増分build

既定のGPU主試験・媒体builder・一括probeは引き続きtone診断構成。
EnginePcm候補を明示選択する場合だけ、別の出力先で次を使う。

```powershell
.\tools\ci\build-mt8167-first-playback.ps1 -EnginePcm -IntegratedUsb -IntegratedWifi -SkipPeripheralBuild -OutputDirectory .\build\engine-pcm-retained
```

初回は新しいEnginePcm親とaudioをbuildする。旧tone client・USB/Wi-Fiの再compile、
hardwareprobeの自動登録は行わない。一致する既存EnginePcm親出力を再利用するときだけ
同じコマンドへ `-ReuseParent` を追加し、flags/SYS hashを照合してaudioだけをbuildする。
固定tone親は拒否する。再利用親の元source commitはmanifestのparent_source_commitに記録する。
異なる音声モードの出力manifestまたはINFがある出力先はbuild前に拒否する。

Engine INFの版は2026.11.7.1004、署名日付は統合方針の11/06/2026。
これはtone版4 INFの11/06/2026,2026.11.6.1001を変更しない。
停止後もpages/SG/voice/D0を保持し、fabric DMA drain・解放成功を保証しない。
同時clientは1つ、保持容量を超える再openとsticky poison後の再RUNを拒否する。
既存tone hardwareprobeを登録済みのOSへEnginePcmを置く場合も、旧tone clientの
自動起動を有効なままにしない。実機への適用判断・旧登録の解除は主担当工程。

## private SYSTEMレシピのsource統合

main 1e1c87d（copy-only媒体3a562de、Drivers大文字D修正0423968、専用SSH最大330分保持）と、
native SYSTEMレシピb7b9a87→4a879f9を統合。保持jobは既定OFFの明示dispatch用で、
この統合ではrunner起動・接続・private媒体生成を行わない。
候補hive・SYSTEM.LOG・backup・private ESP/WIM・private resultは公開repoとCI artifactへ含めない。
公開対象はレシピとテンプレートだけ。private出力は作業ルート内のrepo外へ保存する。

全SYSTEM監査は保護keyのACLで阻害されたため実施していない。
検証範囲はSelect、およびDeviceInstall/RpcSs直下のraw値fingerprint、対象serviceの
Type/Start/ErrorControl/ImagePath/ObjectName/DisplayName/DependOnServiceの7値だけ。
Select.Current/Defaultと依存serviceのType/Startを確認し、関連既存設定はnative replay後を
基点として登録前後とclean candidate再loadで照合する。子keyへ再帰せずACLも変更しない。
recipeのresult/worker manifestはwhole_system_audit=falseとaudit_scopeを明示する。
この限定検査を全SYSTEMの無変更証明と呼ばない。

主担当申告では4a879f9のWindows native準備、clean format 1.5、LOG無し再loadを確認済み。
run25はprivate保守ESPで適用中という情報であり、この統合作業での実測・起動成功ではない。
候補hiveのhashや中身、バックアップ・private媒体は本source記録へ転記しない。

## EnginePcm ARM mapping修正

既定OFF候補へde214c8のWC/Normal-NC検査を取り込み。329413a以前の公開buffer cache設定を
通常AudioSrv用として採用しない。kernel/userのMmWriteCombined要求と、CPUアクセス前の
全page ATS1CPR/PAR検査を使う。PARはstage-1のみで、user PTE/EL2の最終属性は未実測。
SYS source=de214c8、署名INF/manifest source=e4dff2b（11/06/2026,2026.11.7.1004）を区別する。
親/audioのWDK compile/link成功は主担当申告であり、この統合で再buildしていない。
GPU主試験/tone診断の基点と、EnginePcmへの旧tone probe自動登録禁止を維持する。

## 次bulkのHardwareProbe準備

27ad206のSystemRequired power requestと、TeslaのHardwareProbe固定選択c98875bをsource統合。
主担当申告でc98875bのnative処理はverified-clean-system-services-onlyまで成功している。
私用SYSTEM/resultの内容やhashを公開sourceへ転記しない。
次bulkはprepared SYSTEM/hash/native manifestとprobe専用EXE/manifest/certを入力し、
SYSTEMをWinPEでRegLoadせず、DISM前にコピーする。旧clientのcertとprobeの新certを区別する。
平坦化回収のSYSTEM/result.jsonもhash pin照合で受け付け、private出力だけに候補を含める。
詳細CLIと検証範囲はtools/winpe/README-mt8167-shared-io-update.mdを参照。
この作業では私用媒体の生成・端末/runner操作を行わない。

## 既定OFFのbrightness公開ソース候補

CarverのWindows0c73f27を統合し、機種UEFI18d8cdeeを独立した同名候補branchへ配置した。
UEFI repoはhttps://github.com/mouseos/uefi-a04br3、18d8cdeeの親は現在のcc7c20faそのもの。
機種packageのDisplayProvider.asl、build wrapper、説明、既存resource確認だけを変更し、
EDK2共通コード・submodule・現在のUEFI checkoutは変更していない。

WindowsはA04_MT8167_DISPLAY_BRIGHTNESS=1、機種UEFIは
A04_DIAGNOSTIC_DISPLAY_BRIGHTNESS_RESOURCES=1を明示したときだけ有効で、両方とも既定OFF。
DISP_PWM0のMMIOを既存MOU0007へ排他割当てした場合だけWindows callbackを提供する。
古いbootに無い領域を固定mapせず、clock/PLL/GPIO1/PMICの所有を奪わない。
Windows display INFは11/06/2026,0.0.1.3。brightnessの実機UI/物理応答・署名・PnPは未確認。

current run26は26d5520固定の既生成媒体のまま。この追加ソースをrun26の入力・バイナリ・
manifestへ差し替えない。次のWDK/機種UEFI build候補としてsource pinだけを記録する。
主担当が次のUEFI buildでbrightnessを有効にするかを決め、両flagとresource構成を一致させる。

## 通常AudioSrv/WASAPI専用probeの次構成候補

Herschelの1e481aeを新規C・単独build helper・READMEの3ファイルとして収録した。
`tools/ci/build-mt8167-audio-engine-probe.ps1` はEXEだけを生成し、driver SYSを再compileしない。
起動は引数なしの `mt8167-audio-engine-probe.exe`。EnginePcm候補向け通常Windows専用で、
既存tone service・KS client・current run26媒体・自動登録へは追加しない。

対象はDeviceTopology/InstanceIdでMT8167\AFEと確認したactive endpoint一つだけ。
2回の独立WASAPI client lifetimeで固定PCMを送り、API完了・clock増加・DMA CUR観測を
分けて記録する。default endpointへのfallbackや任意dispatcherはない。
可聴は常に未確認、実kernel pin再openやDMA内容CRCを測定したとは扱わない。
manifestのexpected SYS hashは回収bundleの期待値であり、実ロードdriverの照合結果ではない。
通常OSのUART名が存在するとは仮定せず、diskログ/stdout/終了状態と通知失敗を区別する。
WDK compile/link・実endpoint/API・実再生の確認は主担当が次構成で行う。
詳細は `tools/audio/README-mt8167-audio-engine-probe.md` を参照。

## probeのUART任意化と旧toneの前提

通常音声専用probeのd0eb436を取り込み（package_version=2026.11.7.2）。
必須diskログの作成・書込み・flushを確認できれば、UART open/send失敗があっても
AudioSrv/WASAPI→DMA観測へ進む。各pass Start前のdisk flushは必須で、保存失敗は開始拒否/exit5。
UART失敗はtransport/uart-optional/output-state/endへ分離し、API/DMA maskやexitへ加算しない。
例：summary=3,3,0、output-state=0,2,0、end result0/UART2は、音声API/DMA確認とUART名不在を区別する。
可聴成功を示すものではない。

旧tone clientは変更していない。tools/winpe/mt8167-audio-first-playback.c:184は
open_uart失敗でresult=3として、ksuser load/KsCreatePin/RUNより前に終了する。
216にもRUN直前のuart_lost拒否がある。run25同様のdevice名不在が続けば旧toneは再生前に止まり、
その結果を音声再生失敗や実RUN済みの証拠にしない。
この任意UART修正は新EnginePcm専用probeだけで、旧tone service/SYS/INF/boot/current run26媒体へ
混ぜない。旧tone診断の通知前提を解消する修正が必要なら、主担当が別の固定client更新として扱う。

## 明示Engine service/media次構成

Toneは既定のまま。service helper `-AudioMode EnginePcm` と媒体builder `--audio-mode EnginePcm` は、
固定別名MT8167EngineProbe/System32\MT8167EngineProbe.exeと専用clients/logs/attempted.binを使う。
旧MT8167HardwareProbeの使用済みkey/path/markerは変更・削除・再武装しない。
native recipeへ追加した固定第3名だけを選び、run27 fresh backupからEngine単独candidateを作る。
旧HardwareProbe向けresultはEngine媒体で拒否する。監査は関連3key＋選択service7値に限定する。

Engine serviceは旧WLAN＋d0eb436専用WASAPI probeを各一回、引数なし、WinVerifyTrust後に起動する。
SYSは固定de214c8、INFはe4dff2b、USB/Wi-Fi/WLANは旧f17b743-a016d53 bundleから検査済み分だけを選ぶ。
旧parent/旧audio/旧tone clientは新媒体の選択リストへ入れない。component別source/hash/INF version/certを記録。
Engine service/parent/audio/clientのBCBF certと、旧USB/Wi-Fi/WLANの2655 certを別信頼として保持する。
署名済みclientはCI helperで再署名しない。Engine serviceだけの新しいWDK/署名buildが必要。

媒体はprivate SYSTEM copy-before-DISM/freshbackup/boot_id方針を維持し、単一parentを選ぶ。
旧service通知だけでEngine全体の完了を推定しない。新service/runid/child別終了/API/DMA/disk証拠を使う。
current run26r2の既生成媒体・実行は変更しない。source-only更新で、runner/端末は非操作。

Engine専用optional --brightness-package/--toprgu-packageは指定時だけ固定DISPLAY/TOPRGUを追加する。
brightness0c73f27とTOPRGU0821853はBCBF certで、各INF版を別々に照合する。
TOPRGUは4CPU/HANDOFF1/DEADLINE0/THERMAL0/UART1と旧normal同一7 kernel blobの固定manifest契約を検査。
bootbrightness341894cdは主担当の前提候補として記録し、このbuilderはbootを生成・適用しない。
Wi-Fi排他のBT c715ad2と後続audio runtime f64b4b4は今回採用せず、固定bundleを維持する。

詳しい入力・固定パス・build手順はtools/winpe/README-mt8167-hardware-probe.md、
README-mt8167-shared-io-update.md、tools/recovery/README-offline-system-service.mdを参照。

Engine serviceは固定MT8167\AFE、MT8167\USB20、A04BR3\MT6630SDIOだけを標準PnP APIで各1回再bindする。
固定packageを専用drivers以下にcopyし、CAT署名を検査する。before/after CM status/Problem/照会エラー/一致数と
API/Win32/rebootをdiskへ記録。3010は後続更新とclient開始を停止し、自動reboot/marker削除/旧tone再armはしない。
USBのdevice role開始拒否等は結果に残し、再起動不要ならWLAN/音声診断を続ける。親は再bindしない。
選択Engine親も旧f17親も同DriverVer=2026.11.6.1001であり、DISM stagingだけでは実ロード親の交換を保証しない。
主担当は実ロードhashを別途確認する。PnP API成功、CM_STARTED、実機I/O成功は別々に評価する。

Engine媒体は明示 `--wifi-package` で37a5bb1の署名済みmt6630-runtime単体へ差し替え可能。
未指定は旧f17 Wi-Fiのまま。source37a5bb1、INF11/06/2026,2026.11.6.1001、署名後hash、BCBF certを検査し、
旧USB/旧WLAN client2655とEngine親を保持する。overrideに別親/clientを混ぜない。
9d3a889のNDIS direct OID登録修正を含む候補だがstatus31の正確なNTSTATUSは未確認。
回収package候補はport/xnu/out/windows-rt-wifi-registration-37a5bb1。manifest契約は媒体READMEを参照。
