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

## 読取り専用理由照会（1103後段候補）

既存READ_SOCKETのschema1/16byte・資格・失敗statusはそのまま保持する。STATUS_DEVICE_NOT_READYはadmission/資源/資格の複数条件で共用され、Win32 ERROR_NOT_READY(21)だけでは原因を区別できない。CM Started/Problem0もGPIO資格を証明しない。既存clientが失敗した段階を出力しないため、既存ERROR行だけでCreateFile/IOCTLのどちらかを断定しない。

新しい `IOCTL_MT8167_ACCDET_QUERY_STATUS` (0x00226558) は同じinterface・FILE_READ_DATA・power-managed queueで、固定68byte/schema1の診断envelopeを返す。GPIO19のDIR/MODE/IES/PULLEN/PULLSEL/DINの固定6fieldを2回読むだけで、GPIO19以外はmaskする。valid bit0..5がfirst、bit6..11がlastの有効read。resultはsocket enumで、成功診断envelopeをsocket/挿入成功と混同しない。reasons bit0=inputでない、1=mode0でない、2=IESなし、3=pullなし、4=pull-up selectなし、5=二重read変化、6=read失敗、7=admission拒否、8=mappingなし、9=offline、10=poison、11=removed。admission/資源NGならMMIOは読まない。

`mt8167-accdet-status.exe --status` は唯一のinterfaceを選び、この固定照会を1回だけ行う。ACCDET_STAGE 1=args/2=interface set/3=enumeration/4=detail・uniqueness/5=open/6=IOCTL/7=ABI validationを出力し、result/reasons/valid/fieldを記録する。未準備resultではERROR21で終了し、Connectedを出さない。2秒timeout→cancel→2秒join、join不能ならERROR_TIMEOUTを出してprocess終了する。通常READ_SOCKETとの自動反復や、資格不足を解消するmux/pull/PMIC writeはない。

親1103は1102にこの診断だけを追加した独立候補であり、AFE jack1027は1102でも使える。未確定の資格を緩めず、RTC/thermal/USB/Audio ownerと既存flagsを保持する。次の主担当の一括更新へsource単位で統合する。現在の親を旧基点へ巻き戻さない。

## GPIO19 stock biasと単一親D0 lease（親1108後段）

この節は旧1102/1103の「内蔵pull-up必須」「親はGPIOを書かない」という制限を、本機のstock根拠で更新する。IOCTL/schemaとactive-low sampleの意味は保持する。

stock LK ELF SHA256 `988e405def2a13778023becbe5b78f8b34a119ef3950d7760e1129d33561e224` のGPIO初期化0x41e008ccは、GPIO bank1 DIR=0x03c3、PULLEN=0xfc20、PULLSEL=0x4020を設定する。GPIO19 bit3はいずれも0で、MODE table0x41e39900のGPIO15..19 entryも0。REA Evidence `ev_4a1f7e6cd99d015ddea4ac13ebd92021bb8270a85458065fd83403ada2c2c951` と `ev_46094c8ea2cd5fbc297922c1653f60980033ac6ecb1f2d4a09a608297df95932`。静的な初期化契約であり、外付け抵抗の部品値や実音声成功を証明するものではない。

上記CT3 sourceのaccdet_setup_eintはgpio_request/input/debounce/IRQを要求する。pinctrl-mtk-common.c:846 gpio_request_enableと:957 irq_request_resourcesはmode0/input/IESを設定し、pullは変更しない。実stock-kernel.dtsのaccdetもGPIO19 active-low、pinconf_default/audiodefaultは空。Linuxのgeneric bias APIが存在することだけでは、このpinのpull-up指定の根拠にならない。本機XNUのUniXNU-a04br3 package/PlatformExpert/Inputとkernel IOKit ARMを照合した範囲では、独立したGPIO19 bias producerは見つからなかった。

`gpio_lease.h`の状態は既存shared親contextにのみ置く。D0開始の閉鎖admissionとpwrapLock下でDIR/input・MODE0・同一bias tupleの二重snapshotを検査し、Linux同等のinput/mode0/IESをmask更新・readbackする。foreign mux/outputは取得しない。stockのPULLEN/PULLSEL=0/0と旧来の1/1のみ受容し、片側だけの変更は拒否する。PULLEN/PULLSEL/DOUT writeは一切追加しない。socket・HP readiness・HP ReadJackはこのlive leaseを必須にし、実DINの二重読取りを保持する。

IES0x910 bit10はGPIO18..20共有なので、個別read後やHP stop時には解放しない。親全体のadmission閉鎖とrundown終了後、同じmutex下でlive状態を検証して保存maskを復元する。他pinのbitsは保持する。read/write/readback失敗やsurprise removalはpoison/retainとしてmappingを解放しない。取得拒否はACCDET局所に限定し、他の親deviceのD0開始を失敗させない。理由bit12はlive leaseなし、bit3/4はそれぞれ0/1・1/0の不一致を示す。

親1108はHP ABI version2/build1107・thermal V3 build1104を保持し、既存署名audio1030と組み合わせる後段候補。現親1104/next132媒体やMencius AFE open-error修正は変更しない。IRQ/debounce通知の実装ではなく瞬時sampleであり、実機HP再生は未確認。stock0/0のrun129 snapshotは理由0x18の原因だった旧条件に一致するが、新版の実機成功は通常batchの観測待ち。
