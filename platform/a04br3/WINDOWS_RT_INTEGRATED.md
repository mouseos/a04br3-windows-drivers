# TAB-A04-BR3 Windows RT統合ソース

独立branch `work/windows-rt-integrated-20261107` はGPU/MMA8652 `d5f676d` と、
USB/audio/Wi-Fi単一parent `f17b743` を履歴を保持して統合する。
KS clientの9 GUID修正はHerschel `a016d53` と同内容の `f035c59` を含む。
正確なsource pinは `windows-rt-integrated-sources.json` とmerge commitの親に記録する。
main checkoutの未コミットファイルと `.task-tmp` は利用しない。

Makefileの末尾追加競合は両方のtarget/recipeを保持して解消した。
GPU/MMA8652はGPU基点、parent/audio/USB/Wi-Fi/KS clientはperipheral基点の
実装を変更せず保持する。新しいMMIO操作、ACPI変更、EDK2変更は追加しない。

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

主担当申告で現在再署名・回収中のperipheral packageは、SYS=f17b743、
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
