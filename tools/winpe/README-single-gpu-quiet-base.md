# 次回singleGPU用のquiet準備WinPE選択

run99/source9cc23714と完成媒体には変更しない。runtime/service/policy/native sourceの変更無し。
既存fixed original baseを既定とし、明示private/manifest/member receiptが揃う場合だけ、
3f19af59生成の既存quiet WIM/ESPを選択する。任意baseへのhash gate緩和ではない。
selectorとWDK検証recipeはfed01547のbyteをそのまま取り込む。

base内flat a04-msdc-probe.inf/SYS/CAT/observer/certとWinPE公開cert storeだけが既存生成時の変更。
通常StartIo/blocks/成功CDBと高頻度DMA poll traceを既定OFF、error/init/checkpointは保持。
DMA完了bit/PIO/timeout/clock/ACPIは不変。速度改善量は未測定、実機選択も未検証。
Provider/AJTD4R名称の未署名templateはこのbaseに含まれない。

この変更はbuilder入力準備だけ。次回main返却freshとGPU候補を明示pinするまで
新native SYSTEM/OS transaction/実行可能な次試験媒体を作らない。
source9ccのfresh98/run99 policyは凍結したままであり、次試験への旧fresh流用を認めない。
