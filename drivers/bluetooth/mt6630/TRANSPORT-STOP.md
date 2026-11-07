# MT6630 BT 2006: protocol OFFをlifetime失効前へ接続

## ソースで確定した不具合

`BtStop` → `a04_wifi_life_stop` は `state=STOPPING` と
`transport.ready=0` を先に設定する。その後の `drain_io` が
`a04_bt_disable` → CHLPCR read → `a04_wifi_life_cmd53` を呼ぶと、
同関数の `ready()` が `LIFE_IO` を要求するため、実SDIO送信前に拒否する。
従来のportable BT fixtureはlifetimeを経由しないため検出していなかった。

2006では要求受付を閉じ、timerとmanual READ queuesをjoinした後、IRQをdrainし、
まだlifetimeがIOの間に既存の実WMT function-off ACK／TX completion／fresh RX-empty／
部分H4・WMTなしを確認する。その後だけ共通lifetimeを停止する。
`drain_io` は先に得たqueue停止証拠を検査し、既存board power-offへ進む。
OFF失敗時は親leaseを返却しない。共有CMD52/53 gateは緩めない。

## 維持する境界

実transportはMSDC2 SDIO function2、STP task0 H4。Bluetooth用UARTではない。
Linux参照はa04br3-linux-drivers commit
`3d2c74e252d6f34a5980deda96bdc04064108d74` の
`a04br3_mt6630_bt.c:mt6630_bt_close`（queue停止→実WMT function-off）と
`a04br3_mt6630_core.c`。GPL実装はコピーしていない。

親RTC32k、GPIO4 PMU_EN、GPIO7 reset、GPIO68–73 SDIO、MSDC2/IRQ141は
既存leaseとboard実装を利用。FTDI、MMIO値、PMIC、親profileを変更しない。
Wi-Fi1012/1013の選択・SYSを変更しない。排他的BT profileを現Wi-Fi共存候補とは扱わない。

Windows8.1標準BTHXは `MaxScoChannels=1` / `ScoSupportHCIBypass` が必須。
MT6630 FC72 bit仕様・codec/FIFO latency・AFE DMA drainが未確定なので、
既存None/0を維持し、標準stack互換・HFP成功とは表示しない。
共存combo broker、実AllMap BT_Addr、OS HCI Reset後の設定保持、RF/pairingも未確認。

一次仕様:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthxddi/ni-bthxddi-ioctl_bthx_query_capabilities
https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/transport-bus-driver-for-bluetooth-power-control-handling-guidelines

## 限定検証

新しい `tests/mt6630_bt_stop_test.c` は実lifetime/CMD53 gateとBT transportを連結する。
旧順序ではOFF送信0・unmap0・release0、新順序では実fixture ACKと排出後のみ
unmap/release各1、証拠欠落・SDIO失敗ではrelease0を確認する。
これは合成SDIO応答による実行検証であり、実機停止成功ではない。

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror tests/mt6630_bt_stop_test.c \
  drivers/bluetooth/mt6630/transport.c drivers/network/mt6630/lifetime.c \
  drivers/network/mt6630/direct.c drivers/network/mt6630/transport.c \
  drivers/network/mt6630/identity.c -o build/mt6630_bt_stop_test
./build/mt6630_bt_stop_test
make build/mt6630_bt_transport_test
./build/mt6630_bt_transport_test
```
