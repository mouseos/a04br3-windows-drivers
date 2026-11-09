# MT6630 shared provider1015: file cleanupとchip merge設定

実機・実接続・通信・SCOは未確認。共有BT client2008の署名bytesとNone/0広告は保持する。
親/GPU/audio/媒体は変更しない。A04_WIFI_BT_BROKER=0の従来Wi-Fi profileは保持。

## 通常要求でcleanupを落とさない

旧providerの単一request slotはREAD/WRITEがあるとIRP_MJ_CLEANUPもDEVICE_BUSYにした。
最後のhandleのcleanupが完了しても、BT側のFileObject参照/function-onが残り得る。

1015は通常要求とcleanupに別slotを持つ。registry lock下でcontrol FileObjectを一つに限定し、
cleanup開始で新しい通常要求の受付を閉じる。既に受けた通常要求を同じNDIS workerで排出後、
cleanupの実WMT OFFを処理する。重複cleanupで元IRPを置き換えない。
FileObjectのCLOSE通知前に別のcontrol fileを入れず、raw pointerを参照し続ける新たなrefは取らない。
既存の実BT所有者refは、fieldをNULLにしてからdereferenceする。

work-item allocation失敗時もcleanupを保持してIdleを閉じ、既存DPCから再queueする。
Haltは受付/タイマー/DPCを止めた後、workerがいない場合の保留IRPを排出してからIdleを待つ。
Halt中のfile cleanup成功はソフトウェアrundownだけであり、物理OFF成功ではない。
実BT停止/親lease解放は従来のjoin後の実ACK/排出/失敗quarantine契約を維持する。
新しいtimeout延長・OFF成功値の偽装・別SDIO workerは追加しない。

MicrosoftのIRP_MJ_CLEANUP（handle終了、保留IRP排出）とIRP_MJ_CLOSE（最後の参照解放）を
別イベントとして扱う。非owner/retired fileからのcontrol要求を受理しない。

## stock DAI_PADをcold共有profileへ接続

stock a04br3_defconfigはCONFIG_MTK_COMBO_CHIP_MT6630=y、
CONFIG_MTK_MERGE_INTERFACE_SUPPORT=y。同機Linux固定source
3d2c74e252d6f34a5980deda96bdc04064108d74 のa04br3_mt6630_core.cは
coexistence設定後/FM strap前にDAI_PAD commandと実応答を照合する。
既存BSD A04BR3WMTDownload.hにも同じmerge_interface処理があるが、共有NDIS profileは
そのoptionを0のままにしていた。combo-onlyで1へ接続する。

新しいレジスタ値は作らず、既存MT6630固有44byte WMT command/8byte ACK照合を再利用する。
Wi-Fi開始後の曖昧なruntime register変更ではなく、同じcold WMT startupの順序内で実行する。
host GPIO36–39/AFE/MRG/DMA/codecは操作しない。これはchip padの要求とACK確認経路であり、
FC72 bit/rate/master-clock、codec latency、方向別DMA drainやSCO bypass資格の証明ではない。
未知のPCM値や他チップの設定は流用しない。実機ACKは未観測なので未確認のまま記録する。

## 限定回帰

tests/mt6630_combo_lifetime_test.cはproductionと同じbroker_lifetime.hを使い、実行中/queued通常要求と
cleanupの順序、retired file拒否、重複cleanup、CLOSEまでの再open拒否、scheduler OOM時の
cleanup保持を合成tokenで実行する。Windowsの実PnPやWMT応答ではない。

```sh
mkdir -p build/tmp
TMPDIR="$PWD/build/tmp" cc -std=c11 -O2 -Wall -Wextra -Werror tests/mt6630_combo_lifetime_test.c -o build/mt6630_combo_lifetime_test
./build/mt6630_combo_lifetime_test
```

新NDIS layoutを使う13 TUは再compileし、変更のない29 core+BT transport1 objectだけを固定cacheから再利用。
client2008のcode/INF/署名bytesは変更しない。新規SDK構築は不要。
