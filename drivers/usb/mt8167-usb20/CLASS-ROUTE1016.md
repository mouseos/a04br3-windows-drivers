# HCD1016：root binding前提・native route・interrupt interval

1115親/1015署名候補を保持した別branch。root PDOとUSB ROOT_HUB20のPnP列挙は実USBPORTが所有し、root_data/port_status/feature callbacksとinvalidate_rootでinbox usbhubへ接続する。1016はStartでinvalidate_rootサービスを必須にし、不在時にMMIO前拒否する。単にroot IDが出ることをbinding成功とはしない。

実RT8.1 INF一次契約：usbport.inf ROOT_HUB20→ROOTHUB.Dev→usbhub.sys、usb.inf StandardHub.Dev→同節。input.inf Class_03→HidUsb/hidusb.sys、keyboard.inf HID_DEVICE_SYSTEM_KEYBOARD→kbdhid.sys。usbstor.inf Class_08&SubClass_06&Prot_50等→USBSTOR.SYS。標準keyboardはEP0 descriptor/class controlとinterrupt IN、BOT mass-storageはEP0とbulk IN/OUT・halt/toggle回復を使う。HCDはclass policyを代替しない。descriptor/enumerationはUSBPORT/hubへ任せる。

REA原本usbport.sys SHA256 c668b894e7733206f987d85296f5e2d5fb4cf5b06c12ee43ea5afef85d4c31fe：0x206c0はendpoint+128のpropertiesを作り、+158（properties+30）へTT context+8、context不在時ffffを書き、+15a（properties+32）はdevice+6からコピーする。有限hub値を「直近の外部hub」と推測してMUSBへ直接流さない。native zero/ffffだけをOpenとworker admissionの両方で受入れる。HS→HS外部hub下、FS→FS外部hub下のno-TTはこの同じnative function-address経路であり、MUSBのTT hub/portを0に保つ。実バス未検証。FS/LS behind HS hubはTT contextを伴うので拒否し、TT対応を広告しない。TT単一/複数識別・split mask/scheduling・TT buffer clear/abort recoveryは未確定/未実装。

REA 0x206c0→0x22db0はHS bIntervalを1<<min(bInterval-1,5)へ正規化し、interrupt properties.periodへ格納する（LS/FSもpower-of-twoへ丸める）。Linux CT3 musb_host.c:2541以降はFS/LSをlinear、HSをdescriptor exponentでRX/TXINTERVALへ書く。そのためHS period1/2/4/8/16/32をMUSB exponent1..6へ変換する。0/非power-of-two/>32は推測せずadmission拒否。FS/LSの正規化値はlinearのまま保持。

Linux CT3 musb_virthub.c ClearPortFeature(ENABLE)はlogical enableを落とし、別ENABLEレジスタ書込みを行わない。1016もPASSIVE FIFO/REQ停止・health proofを経た後だけenableを落とし、SESSION/VBUSを保持する。失敗時は成功ackにしない。C_SUSPENDはsuspend changeを生成しない現profileなので0のclearだけを受入れ、suspend/resume成功を偽装しない。給電/OC/DMA/QMU/StopAndProve/retainは不変更。

模擬試験と実機成功を分ける。native route/HCD/workerのTT事前拒否、HS32→6、root change latch/invalidation、disable proof不足拒否を検証する。実PC-roleではhost物理条件がないため登録/標準hub binding/keyboard入力/BOT transfer/実5Vは未実測。single active transferにはendpoint間公平性・長いNAK interruptとbulkの競合という残課題がある。通常keyboard+mass-storage同時利用完成とは扱わない。external hub/TT、ISO/high-bandwidth、大packet interrupt、suspend/resumeは別工程。
