# MediaTek MT8167 ACCDET external GPIO19 socket input

本機のヘッドホンsocket検出を、既存shared親のGPIO mappingとD0 admission/lockで読み取る。BTや音声経路を偽装する代替transportではない。GPIO19 external switchだけの瞬時sampleで、PMIC ACCDETのマイク種別/remote button/analog route、debounced hotplug通知、Windows headphone endpointは未実装。

一次sourceはCT3_GPL_LGPL_Src commit7c4782127d4ea11965be00ce1343056be9bfb769:

- arch/arm64/boot/dts/mediatek/a04br3.dts:134のaccdet-gpio19、IRQ_TYPE_LEVEL_LOW、512000us debounce、plugout20ms。
- drivers/misc/mediatek/accdet/mt8167/accdet.cのaccdet_setup_eint/accdet_eint_func。初期plugout時のlow IRQでpluginへ遷移し、次をhighへ切り替える。
- drivers/pinctrl/mediatek/pinctrl-mt8167.cのGPIO19は通常pull（special PUPD対象外）、IES18..20は0x910 bit10、DIR0/DIN0x200/PULLEN0x500/PULLSEL0x600/mux0x300、port shift4。common.cの5pin×3bit modeに従いGPIO19=mux0x330 bits12..14、DIR0x10/DIN0x210/PULLEN0x510/PULLSEL0x610 bit3。

`A04_SHARED_ACCDET_READ`既定0。1を指定すると単一のshared親が専用read-only device interfaceを公開する。PMIC、AFE、GPIO mux/direction/pull、IRQ、clock、WiFi、SD、USBへ書かない。独立mapping/二重GPIO owner/追加PWRAP ownerは作らない。parent1101 sourceを基点とし、8range/RTC/thermal/GPU/capture等を保持する。

IOCTL_MT8167_ACCDET_READ_SOCKETはMETHOD_BUFFERED/FILE_READ_DATA、input無し、output16bytes schema1。mux0/input/IES/internal pull-upを資格として要求し、資格bitsとDINを二重読取りする。値が変わればBUSY、未準備や読取失敗はNOT_READY。成功時だけqualified1/inserted0または1を返す。pinの資格を勝手に修正せず、未知状態でconnectedを合成しない。internal pull-upが無ければ外付けpullの存在を推測して成功化しない。

WDF sequential power-managed queue/PASSIVE callbackから既存ParentLockを使い、online/mapping/poison/removalを検査する。requestは同期的に完了し、追加worker/timer/保持requestは無い。D0終了やremoved後にMMIOを継続しない。省電力中の観測成功は要求しない。lockはmemory参照だけの電源資格とは扱わない。

`tools/input/mt8167-accdet-read.c`は公開interfaceを唯一1つだけ選び、`--read`で1回readするfreestanding Win32 client。任意path/任意GPIO/書込み/繰返しprobeは無い。出力のGPIO19_INSERTED_SAMPLEは瞬時の外部switch sampleであり、物理音声経路/安定挿入/マイクの証明ではない。実機はmainだけ。

この変更を既存parent1101のINF/署名へ混ぜて再包装しない。検証はcompile-onlyとし、最終profileで採用する場合は他の親差分と合わせ新source/上位INF版/SYS/CATを1回build/signする。親1026/旧6rangeへ戻さない。既定OFFではqueue/interface/readは追加されない。

Bluetoothは別branchの[採取RT8.1契約](https://github.com/mouseos/a04br3-windows-drivers/blob/f7a1208b/docs/mt6630-rt81-bthx-contract.md)とsideband sourceを参照する。採取RT8.1 BthMiniはMaxAcl>=0x103/ScoSupportHCIBypass/MaxScoChannels1を必須にする。最新1019/2011/3003もPCM bit/clock、方向別latency、独立DMA drainが未確定のためNone/0を保持し、OS radio開始を成功化していない。Linuxのnative HCI_SDIOが動くことだけからBTHXの能力要件を満たしたとは扱わない。
