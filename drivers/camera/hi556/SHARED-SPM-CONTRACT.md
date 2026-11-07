# camera IMGと単一SPM ownerの境界

GPU domain担当E/Carverと共有する契約案。既存親sourceはread-only参照し、
担当者のsource/機種ACPIへ無断で変更を入れない。
参照parent1014: `287241c37fa0eae538e5e2d136dea5df42de7e43`。
`gpu_platform_interface.h`のV2はGPU専用exclusive leaseであり、cameraへ流用しない。

## 現実装：pinned handoff

Windows cameraはSPM/INFRA/TOP/GPIO/PWRAPをmap/writeしない。
SPM/clock/railは機種専用UEFIのboot時に確立し、camera NVSとACPI HSTAを検証する。
Windows ownerがruntime IMG電源操作を提供した、または省電力対応したとは扱わない。
MMAだけがI2C1を所有する。cameraがmapするのは実PnP割当済みのISP/SENINF、
MIPI analog、LARB1、read-only NVSのみ。

HSTAのACPI readbackはSPM driver resourceの再割当や第二のMMIO writerではない。
既存owner以外にSPM mappingを追加しない。親のGPU off/reset経路はcamera用SPM制御へ
書き換えない。親側が将来ISPをoffできるようになる前に、以下のIMG leaseが必要。

## 調整すべきruntime契約（未公開API）

- 同じ親FDOがSPM/INFRAを一度だけmapし、既存の同じlock/rundownで全SPM操作を直列化する。
- GPU専用tokenと別namespaceのIMG参照付きlease。GPU/IMGが独立domainであることを保ち、
  カメラ保持中にGPUの通常render/off/onを不要にブロックしない。
- AcquirePinnedは実親D0参照を保持し、実IMG状態を確認する。未知/不足割当/世代違いは拒否。
  consumerもPnP INTERFACE参照を保持する。profileの意図だけでAPIをpublishしない。
- Observeは実SPM status0x60c/status2 0x610のbit5、ISP control0x238を同じlock内で返す。
  出力には観測可否、実結果、世代を含める。未観測0をONと扱わない。
- ReleasePinnedは所有参照の解放であり、暗黙のdomain offではない。
  失効/D0 exit時に新規受付を止め、進行中の親callbackを排出する。
- IMG offを後で実装する場合、cameraのsensor stopとISP reset ACKによるDMA停止証拠、
  全IMG consumer/clock voteのrundown、sourceのSRAM/電源ACKを前提にする。
  GPU_OFF/DMA_STOPPEDの既存証明をcameraへ流用しない。
- IRQ/receiver/IMGO DMA bufferの所有者はcameraのまま。親はcamera frame成功を代理応答しない。

register根拠はCT3 `7c4782127d4ea11965be00ce1343056be9bfb769`の
`drivers/soc/mediatek/mtk-scpsys-mt8167.c`のISP domain。
新GUID/ABI/profile名と親側実装commitは担当者との合意後に固定する。
現在のsourceは上記APIの存在を広告せず、未実装broker/stubや第二SPM writerを作らない。
