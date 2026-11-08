# 次の周辺batch（fresh120待ち）

`prepare-peripheral-next-draft.py` は固定署名USB1010/WiFi1020/SD8と、
既知capture ERROR22のbuffer-first修正clientを一つに集約する。
新しいfresh120/main manifest hash/current GPU policy/Recovery boot id/native
selectionが未結合であり、出力は常にdraft/normal_install_allowed=false。
service/header/hive/媒体は生成しない。119媒体を変更しない。

今回のruntime source準備:
- 固定6を生成target数（上限9）へ。DIF対象は生成INSTALL_MASKだけ。
  現行旧6target profileは従来の0x3c（子2件store-only）を保持。
- run115でmetadata先行guardがCM観測まで中断した点を修正。
  devnode発見後はCM rc/status/problemを先に独立記録し、その後のmetadataと
  SYS hashの成否を別イベントに記録する。metadata失敗をstarted成功にしない。
- version/date DEVPKEYのERROR_NOT_FOUNDだけ同じdevnodeのDIREG_DRVへfallback。
  型、長さ、厳密version/date/INF/service/class/SYS/CAT比較は保持。
  ERROR_ACCESS_DENIEDや存在する値の不一致ではfallbackしない。
- Raman source52e71519のnative reader IncludeWifiSdを同byteで取り込む。
  private新clone限定、main指定capture/hash/policy/boot idと原trio不変を要求。

旧initial batchのMT8167PeripheralBatch registrationは現在既存であり、
初回arm guardへ無理に再投入しない。次の専用service identity/既存service保持、
最終3target policyとmask、media/stage/native契約はfresh120実snapshotに結合後に
確定する。旧親/MMA/eMMC/RTC/thermalを再installしない。
Carson DOD回転候補は署名artifact pins到着後だけ追加検討、ACCDET GPIO19は
optionalで親1101の既存flags/資格を保持できる場合のみ。既存parentを退行させない。
現在のmatched8range boot/GPU/UMD/FW/client/Engine/旧logs/SOFTWARE/userfs保持。

USB1010はdirect boot keyboard+mouseの複合interface欠落修正であり、physical5Vや
標準USB host controller/root hubの完成ではない。power変更を根拠なく追加しない。
録音ERROR22は既存source61106acfのSTOP-buffer→ACQUIRE修正を再利用し、
旧clientを再実行しない。搭載audio4facb36eはcapture owner/ADC/VUL/IRQ2配線済み。
latency/drain未資格、standard Audio Engine/実録音/可聴出力は未成立として保持。
このsourceを音声driver修正や音声機能完成とは報告しない。

実機/UART禁止。最終service compile/sign/媒体生成はfresh120結合後に一度だけ行う。
