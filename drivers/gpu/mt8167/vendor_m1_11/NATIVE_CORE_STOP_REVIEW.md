# GPU0内停止／reset保持のsource照合

同版KM: port/gsi/kernel-4.14 eb47f42e1f84fe5716ef239620725f82718a620a、
DDK m1.11ED5425693。binary逆解析ではなく通常source調査。

## 確定している順序

- `rgxpower.c:RGXPrePowerState`はFW off request/ackとePowState OFFを確認し、
  実host ISRでsampleしたIRQ countをFW countへ追い付かせてからRGXStopする。
- `rgxstartstop.c:RGXStop`はSidekick/Garten、SLC idle、BIF/BIFPM MMU requests、
  external reads、SLC_STATUS1をpoll。MTSのthread/DM関連付けを0にする。
  MIPSでは最後にGARTEN idleも必要。FWackだけで終了する経路ではない。
- `rgxinit.c:RGXSoftReset`はSOFT_RESETの各bitがresetするmoduleのmaskだと定義し、
  許容maskを検査し、write64→read64 fence→deassert→read64 fenceする。
- `rgxstartstop.c:RGXStart`の非S7/nonshared-SLC profileのmaskは
  0x00e7fffffffffc1d。GARTEN/MIPS(bit32)、MMU(bit34)、BIF(bit16/33)、
  SLC(bit27)、TA/3D/USC/PDS等を含む。SOFT_RESET2はS7側分岐のみ。
- readbackについてsourceの説明は「fence previous write in order to clear
  the SOCIF pipeline」。GPU0 RW range内だけで行える。

## 実装した範囲

`A04BR3RogueCoreResetHold.h`と`windows_stop.c`にGPU0限定candidateを追加。
既存native admission closure＋実FW off ack＋IRQ counter drainを前提に、
mask readback→reset前RGXStop→SLC flush/inval pending0→full reset write/readback→
IRQ mask維持/clear/quietを確認し、resetを保持する。deassertしない。
reset前pollでBUSYならresetを書かず、reset後のclear済statusを過去のdrain証拠にしない。
reset readbackが必要maskを保持しない、all-ones、IRQ pendingなら失敗してDMA保持。
物理bitのreadback挙動は未実測であり、未観測を成功扱いしない。

`native_owner_irq`は実IRQ claimでFW countをsampleする。off-drain中はFWCCBが
空である時だけ実ISR内でsourceをrearmし、poweroff transportの複数IRQを追う。
close_offはOFF、stable FW count==実sourceからsampleしたhost count、KCCB/FWCCB空、
source deassertを同じ実Dxgk lockで照合してmask/closeする。
これはOS ISR/work-item rundownやDMA停止の全proofではない。

## NoDMAについて未確定なこと

SLC_STATUS1の同版field名はREADS0/1、READS0/1_EXT、PAUSED。
BIF external statusもREADS系。SLC_IDLEにはIMGBV4/RBOFIFO/CBAR等があるが、
このsource/headerだけでは「外部write responseをすべて回収した」の完全な
意味・観測条件を独立に確定できていない。SOCIF fenceのコメントを、
別masterのAXI write-response drainの証明へ拡大解釈しない。

したがって現candidateは実reset保持の観測を返すが、NoDMA、DMA release、
GEO_PROOF_DMA_STOPPED、63capsを返す実装ではない。反証が得られたわけでもない。
reset保持後にhardware_owned/exposedをfalseへ書き換えたり、MDL/SGをfreeしない。
失敗時にも保持する。正常source-pathのAXI completion契約または同版hardware資料が
確定するまでは「FWack/idle/resetだけでfree可能」としない。

## 共有platform側との関係

parent1014別branchにsourceに沿うdomain off/on純moduleとINFRA-only最小hookを
用意したが、ecaa1008/main/UEFIの既存parent割当にはSPMが無い。
現在のhookにSPM・GPUCore・CPU4/watchdogの新map/writeは無く、共有ACPIも変更しない。
GPU0内停止で十分と確定できるならこのparent依存を最終WDDMへ追加しない。
CreateV3/control-v2/全lifetimeの完成は別途継続し、停止未完を成功stubで埋めない。
