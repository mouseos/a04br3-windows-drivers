# MT6630 native HCI / HFP qualification boundary

本候補は既存の単一MSDC2/SDIO function2/STP task0所有者を使用する。
別MMIO map、別WMT owner、AP BTCVSD SRAM/SPI194経路を追加しない。
INF2005は旧2004と別版。既存WiFi排他、親1014 GPU/SD7の凍結byteを維持する。

## FC72を実vendor artifactで確定した範囲

vendor source `0c756f413e7011231c8fb793af0930311023f68d` の
`proprietary/vendor/lib/libbluetooth_mtk.so`、SHA256
`77e897889ea40a09ced2baf0194b92edc97720c963c7a484a8529962e7bea429`。
REA/Ghidraの静的解析で、MT6630専用preload tableの第三関数は
`GORMcmd_HCC_Set_PCM`、canonical entry `0x13f78`（ELF Thumb symbol/raw addressは
`0x3f79`、Ghidra image base `0x10000`）と確認した。
Evidence `ev_f211ab001c45908d967dc911d6c03dd430ae3522357915bbc71ae7c163382f4f`。
`GORM_FW_Init_Thread` のtable選択根拠は
`ev_ef9e53278411e46824f5cb0297478044de26328553b11f84831520e383f5a933`。

関数はopcode FC72、parameter length4、動的btinit offsets0x10..0x13
（GORM record offsets8..11）をbyte順のまま送る。bitのdecode、rate/極性/master-slaveの
変換、codec遅延値の返却はない。Windows transportは既にこの順序で送っている。
本候補は実Command Completeを得た後だけ`pcm_command_applied`を立て、実HCI Reset成功／
function停止で失効する。これは設定適用ACKであってPCMの資格やHFP接続証明ではない。
個体MAC/校正・PCM recordの値をログや成果物へ保存せず、起動時read-only読出しを維持する。

対照一次source: Linux `191c872308f6a175324bc33807852b10363066cb` の
`drivers/bluetooth/btmtk.h:struct btmtk_sco`は4bytesをclock/transmit-format/channel-format/
channel-selectと命名する。しかし同commitの`btmtksdio_sco_setting`はMT7921向けであり、
その0x49/0x80、pinmux、bit意味をMT6630へ流用しない。
同機Linux `3d2c74e252d6f34a5980deda96bdc04064108d74` の
`wifi/mt6630-gen3/os/linux/hif/sdio/a04br3_mt6630_bt.c:133`も4bytesの転送まで。
MT6630 firmware側のfield定義がないためValidatePcmは依然NULLである。

## 実HCI接続eventをnative transportへ接続

[Bluetooth Core HCI events](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html)
のSynchronous Connection Complete(0x2c)、Changed(0x2d)、Disconnection Complete(0x05)を
既存のH4再構成後に処理する。実status、handle、SCO/eSCO type、packet長、interval、
retransmission window、air modeだけをprivate stateへ保持する。BD_ADDRは保存しない。
matching成功eventだけがactiveを立て、matching実disconnectか実HCI Reset成功が解除する。
Command Statusは受付なのでactiveを立てない。未知／重複別handleはuncertainを残し、
新たなSCO能力広告へ転用しない。生event自体は従来どおりBTHX consumerへ渡す。

packet長やslot intervalをcodec/FIFO latencyへ換算して成功値にする処理はない。
HFP request-connect受付と、`d5e9feb2`の実status通知snapshotは引き続き別の状態である。

## queue停止とAFE DMA drainを分離

`a04_bt_disable`は実WMT function-off ACKとTX completionの後、同一SDIO所有者で
CHISRを再読出しし、RX空／TX未完了なし／部分HCI・WMT frameなしをすべて満たすまで
実deadline付きで排出する。満たさなければ失敗を上位へ伝播し、親leaseを解放しない。
FDO D0Exitはrunning admission閉鎖後にtimerをjoinし、両manual READ queueを
`WdfIoQueuePurgeSynchronously`でjoinする。waitはFDO lock外で行う。

これはSDIO/STP/WDF requestの排出契約であり、Audio AFEのDMA MDL停止証明ではない。
CT3 Linux `7c4782127d4ea11965be00ce1343056be9bfb769` の
`kernel-4.4/sound/soc/mediatek/mt8167/mt8167-afe-pcm.c`、dais_trigger STOPは
memif enable解除とIRQ disableまで。100usはMRG enable/disable順序の待ちで、
bus outstanding=0の仕様ではない。CUR固定、IRQ flush、enable readback、時間待ちを
方向別bus-drain proofとして追加しない。Ramanの実providerに
VerifyDmaQuiescentがない間は公開KS factory/HCIBypassを有効にできない。

## latencyと未完了条件

同Linux DAI/codec経路にはMT6630 codec/FIFO delayの数値契約がない。
KSRTAUDIO_HWLATENCYはFIFO bytes、chipset/codecは100ns単位であり、
buffer lengthやHCI packet intervalを同値とみなさない。
CodecLatency=NULL、ValidatePcm=NULL、HCIBypass None/0を維持する。
Windows8/8.1はそのNone/0を標準stack互換として受理しない公式契約のままであり、
この署名SYSを標準radio/HFP/実音声成功候補と表示しない。

未完了: MT6630 firmware FC72 bit仕様、実codec/FIFO latency、Raman親v2/ready通知結合、
実AFE方向別DMA drain。実機でのみ得られる成功を静的解析で捏造しない。
本変更のWDK/SYS/CAT成功と、主担当の標準stack/SLC/PCM実機結果は別に報告する。
