# MT8167 MUSB 給電観測・host成立・role離脱

このbranchは署名親1120 source `0f4a51ba3bbcff090e8f24b30291c7564c41eb48` を基点に、署名HCD1018 source `32444ffe760798576eb7d94e58ccd6b0a924145f` のUSB leaf/testsを統合したもの。親1121／HCD1019はsource上の候補識別であり、署名済みpackageや実機成功を表さない。

## 親／HCDの対応

`MT_MUSB_OWNER_VERSION=3` を双方で要求する。version 2の親と新HCD、新親と旧HCDの組合せを使用しない。観測構造のサイズ・資源範囲・アドレス変換契約は維持。親のclock snapshot ABI1/resource1117と他の周辺機器は変更しない。

親INF候補は `drivers/bus/mt8167-shared/prototype/shared-parent-musb1121.inf`、HCD INFは `drivers/usb/mt8167-usb20/mt8167-musb-hcd.inf`。新規資源やfirmware変更は不要。将来Windows向けにビルドする場合は親本体とHCDを対応させ、header依存closureを再計算する。旧1018用の固定4compile/6reuseレシピをそのまま流用しない。

## 観測と遷移

- 親はsource OFF・既知idle PIOを確認してからPHY sensingを復帰する。SIF+0x800のBC1.1、UART、forced suspend/data/pulldown状態とコンパレータenableをreadbackし、復帰後1ms待つ。停止証明にもこの条件を要求する。
- GPIO IDは通電前の候補条件。SESSION開始後、bus-valid overridesを解除し、DEVCTLのSESSION=1、HM=1、BDEVICE=0を最大100ms確認する。成立前はroot POWER成功を返さない。成立後のHM喪失も転送停止へ進む。既存URB absolute deadlineは延長しない。
- OC、未知のGPIO状態、VBUSERRORをID離脱より先に評価する。正常ID離脱でも直ちにsourceを切り、停止証明とRelease成功までlease／世代を保持する。成功後に世代を更新し、古いPDOの退役とcleanupが終わってから新PDOを生成する。IDが早く戻っても古いPDOを再利用しない。
- source断失敗、停止失敗、OC／VBUS異常のpoisonはcleanupで消さない。source OFFと外部PC由来のVBUSを区別する。DMA/QMU未知状態の書換え許可は追加しない。

根拠は既存BSD board_host.cの復帰処理と、CT3 GPL source `7c4782127d4ea11965be00ce1343056be9bfb769` の `kernel-4.4/drivers/misc/mediatek/usb20/mt8167/usb20_phy.c::usb_phy_recover`、`usb20_host.c::musb_id_pin_work`、`mtk_musb_reg.h`。Linuxのレジスタ契約を用いた既存Windows ownerの修正であり、Linux実装の転載ではない。XNUのdevice-role EP1成功や強制VBUS値をhost給電の実証として扱わない。

## ホスト検証の再現

```
mkdir -p build/tmp
export TMPDIR="$PWD/build/tmp"
make CC=clang test-mt8167-musb-owner
make test-mt8167-musb-host-models
make CLANG='clang -fsanitize=address,undefined -fno-omit-frame-pointer' test-mt8167-musb-host-models
make CC=clang test-mt8167-usb-pnp-resources test-mt8167-usb-host-core test-mt8167-usb-host-control
```

`host_include`は既存の決定論的USB試験をLinuxで実行する型定義だけで、Windows ABI／WDK検証の代わりではない。OS／MMIO seamは既存試験のものを使う。新しい実機probeやテストdriverは追加しない。

実Windows USBPORT登録、PnP通知・削除の実際の順序、host列挙、5V供給、給電改善は未検証。Windows/ARMビルド・署名も本branchでは未実施。親WDF全体の既存host targetは同梱stubの `wdmguid.h` 不足により実行できていない。実機採用には別途mainによる対応packageの検証が必要。
