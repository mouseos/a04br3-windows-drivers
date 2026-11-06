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
