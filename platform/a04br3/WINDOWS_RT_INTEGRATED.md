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
