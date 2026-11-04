# TAB-A04-BR3 MT6630 / MSDC2 Windows向け基盤

このディレクトリはWi-Fiドライバではない。無線・SDIOコントローラにアクセスせず、
未完成の所有権や電源処理が実機上で有効になることを防ぐ。`wdf_sdio_probe.c` は
常にPrepareHardwareを失敗させ、MMIO、GPIO、送受信、NDIS登録を行わない。
対応する機種ACPI候補は別UEFI worktreeの `WifiSdioCandidate.asl` で、_STA=0かつ
どのfirmware volumeにも組み込んでいない。`MOU0011` は内部候補IDであり製品IDではない。

## 確認した事実と限界

- 端末のLinux DTS `arch/arm64/boot/dts/mediatek/a04br3.dts` はMT6630コンボ、
  MSDC2 4-bit SDIO、PMU_EN GPIO4、RST GPIO7、Wi-Fi EINT GPIO5を記述する。
  `mt8167.dtsi` のMSDC2は0x11170000+0x1000、GIC SPI109 level-low
  （ACPI GSIV 141）。共有INFRACFG reset領域、pinmux、時計はこの専有領域に含まれない。
- 実機過去記録 `port/gsi/logs/ios-feasibility/wifi-worker/implementation-report.txt`
  はCIS vendor 037a、device 6630、2 functionsを観測。CT3 Linux `sdio_detect.c` も
  037a:6630をMT6630と列挙する。これは同一端末の過去記録であり、Windowsで再取得した
  値ではない。MACアドレス・校正値をソースやバイナリに含めない。
- 機種UEFI付属DTSでは固定regulator 3.3V、PMU4、reset7 active-low、外部32k RTC、
  100ms startup、30ms post-powerが記述される。これは構成されたDTS値で、回路実測や
  電源操作の安全性の証明ではない。Linux 4.14端末DTSの200MHz上限と当該UEFI DTSの
  50MHz上限は別構成で、いずれもWindows用の承認済み設定ではない。
- 過去XNU成果はCMD52/53とMT6630のWMT/firmware研究を示すが、WindowsのSDバス、
  IRQ、同時アクセス、低電力遷移やネットワークの動作を証明しない。firmware選択は
  silicon revisionとの一致、配布権、端末固有校正を別途確認する。

## Windowsの境界

Microsoftの [SDカードドライバスタック](https://learn.microsoft.com/en-us/windows-hardware/drivers/sd/sd-card-driver-stack) と
[SDバスインターフェイス](https://learn.microsoft.com/en-us/windows-hardware/drivers/sd/opening--initializing-and-closing-an-sd-card-bus-interface)
に従い、ホストコントローラ提供とSDIO機能クライアント、NDIS無線miniportを別の
責任として設計する。手元WinPEにはARM版 `sdbus.sys` と `ndis.sys` が存在するが、
MSDC2ホストが利用可能であることやMT6630用Windows ARM32 WLANドライバの存在を示さない。
MT6630向けの利用可能なWindows RT ARM32ドライバは今回の一次資料調査では特定できていない。
デバイス固有ネットワークIDやMT6630と無関係な既存ドライバを推測して流用しない。

## 実装部品と検証

`identity.c` は供給されたCISのMANFID tupleを境界・重複検査し037a:6630を判定する。
`sdio_args.h` はCMD52/53引数とR5エラーを純粋な計算として扱い、送信はしない。
`ownership.h` はhost、共有clock/GPIO、コンボ機能、firmware、IRQの各条件と障害後の
poisonを管理する部品であり、真の排他やpower lifetimeの証明ではない。
`make build/mt6630_identity_test build/mt6630_sdio_args_test` の後に両テストを実行する。
ARMクロスコンパイルとASan/UBSanはホスト模擬で、無線操作・ネットワーク成功ではない。

実装にはWindowsのMSDC2ホスト／SDIOバス、共有クロックとresetのブローカ、GPIO/EINT5
割込み、電源・32kとBluetooth等の共同所有、firmware／校正の動的取得、NDIS無線
miniport、電源／PnP停止と安全な復旧、署名・実機検証が残る。条件が揃うまでSTAや
PrepareHardwareの安全ゲートを外さない。無線設定・MAC・国別RF設定を推測しない。
